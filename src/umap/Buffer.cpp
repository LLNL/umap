//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

#include <pthread.h>

#include "umap/Buffer.hpp"
#include "umap/FillWorkers.hpp"
#include "umap/PageDescriptor.hpp"
#include "umap/RegionManager.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {
  Buffer::Buffer( void )
    :     m_rm(RegionManager::getInstance())
        , m_size(m_rm->get_max_pages_in_buffer())
        , m_waits_for_avail_pd(0)
        , m_waits_for_state_change(0)
  {
    m_array = (PageDescriptor *)calloc(m_size, sizeof(PageDescriptor));
    if ( m_array == nullptr )
      UMAP_ERROR("Failed to allocate " << m_size*sizeof(PageDescriptor)
          << " bytes for buffer page descriptors");

    for ( int i = 0; i < m_size; ++i )
      m_free_pages.push_back(&m_array[i]);

    pthread_mutex_init(&m_mutex, NULL);
    pthread_cond_init(&m_avail_pd_cond, NULL);
    pthread_cond_init(&m_state_change_cond, NULL);

    m_evict_low_water = apply_int_percentage(m_rm->get_evict_low_water_threshold(), m_size);
    m_evict_high_water = apply_int_percentage(m_rm->get_evict_high_water_threshold(), m_size);
  }

  Buffer::~Buffer( void ) {
    std::cout << m_stats << std::endl;

    assert("Pages are still present" && m_present_pages.size() == 0);
    pthread_cond_destroy(&m_avail_pd_cond);
    pthread_cond_destroy(&m_state_change_cond);
    pthread_mutex_destroy(&m_mutex);
    free(m_array);
  }

  //
  // Called from FillWorker threads after IO has completed
  //
  void Buffer::make_page_present(PageDescriptor* pd) {
    lock();

    pd->set_state_present();
    wakeup_page_state_waiters(pd);

    unlock();
  }

  //
  // Called from EvictWorkers to remove page from active list and place
  // onto the free list.
  //
  void Buffer::remove_page( PageDescriptor* pd ) {
    lock();

    pd->region->erase_page_descriptor(pd);

    m_present_pages.erase(pd->page);

    pd->set_state_free();

    pd->page = nullptr;

    //
    // We only put the page descriptor back onto the free list when
    // if it isn't deferred.  Note: It will be marked as deferred when
    // a range has been unmapped and all of its pages get flushed.  It becomes
    // undeferred by when the eviction manager takes it off the end of the
    // end of the buffer queue later.
    //
    if ( ! pd->deferred )
      release_page_descriptor(pd);

    unlock();
  }

  void Buffer::release_page_descriptor( PageDescriptor* pd ) {
      m_free_pages.push_back(pd);

      if ( m_waits_for_avail_pd )
        pthread_cond_signal(&m_avail_pd_cond);

      wakeup_page_state_waiters(pd);
  }

  //
  // Called from Evict Manager to begin eviction process on oldest present
  // page
  //
  PageDescriptor* Buffer::evict_oldest_page() {
    PageDescriptor* rval = nullptr;

    lock();

    while ( m_busy_pages.size() != 0 ) {
      rval = m_busy_pages.back();

      // This was previously removed by evict_region, so there is no additional
      // state work to be done
      //
      if ( rval->deferred ) {
        UMAP_LOG(Debug, "Deferred Page: " << rval);

        wait_for_page_state(rval, PageDescriptor::State::FREE);

        m_busy_pages.pop_back();
        m_stats.pages_deleted++;

        //
        // Jump to the next page descriptor
        //
        release_page_descriptor(rval);
        rval = nullptr;
      }
      else {
        UMAP_LOG(Debug, "Normal Page: " << rval);
        wait_for_page_state(rval, PageDescriptor::State::PRESENT);
        m_busy_pages.pop_back();
        m_stats.pages_deleted++;
        rval->set_state_leaving();
        break;
      }
    }

    unlock();
    UMAP_LOG(Debug, "Returning: " << rval);
    return rval;
  }

  //
  // Called from uunmap by the unmapping thread of the application
  //
  // The idea is to go through the entire buffer and remove (evict) all pages
  // of the given region descriptor.
  //
  void Buffer::evict_region(RegionDescriptor* rd) {
    lock();

    rd->set_evict_count();

    while ( rd->count() ) {
      auto pd = rd->get_next_page_descriptor();
      pd->deferred = true;
      wait_for_page_state(pd, PageDescriptor::State::PRESENT);
      pd->set_state_leaving();
      m_rm->get_evict_manager()->schedule_eviction(pd);
    }

    unlock();

    rd->wait_for_eviction_completion();
  }

  //
  // Called by Evict Manager to determine when to stop evicting (no lock)
  //
  bool Buffer::evict_low_threshold_reached( void ) {
    return m_busy_pages.size() <= m_evict_low_water;
  }

  //
  // Called from Uffd Handler
  //
  void Buffer::process_page_event(char* paddr, bool iswrite, RegionDescriptor* rd)
  {
    WorkItem work;
    work.type = Umap::WorkItem::WorkType::NONE;

    lock();
    auto pd = page_already_present(paddr);

    if ( pd != nullptr ) {  // Page is already present
      if (iswrite && pd->dirty == false) {
        work.page_desc = pd;
        pd->dirty = true;
        pd->set_state_updating();
        UMAP_LOG(Debug, "PRE: " << pd << " From: " << this);
      }
      else {
        UMAP_LOG(Debug, "SPU: " << pd << " From: " << this);
        unlock();
        return;
      }
    }
    else {                  // This page has not been brought in yet
      pd = get_page_descriptor(paddr, rd);
      pd->has_data = false;
      work.page_desc = pd;

      rd->insert_page_descriptor(pd);
      m_present_pages[pd->page] = pd;

      if (iswrite)
        pd->dirty = true;

      UMAP_LOG(Debug, "NEW: " << pd << " From: " << this);
    }

    m_rm->get_fill_workers_h()->send_work(work);

    //
    // Kick the eviction daemon if the high water mark has been reached
    //
    if ( m_busy_pages.size() == m_evict_high_water ) {
      WorkItem w;

      w.type = Umap::WorkItem::WorkType::THRESHOLD;
      w.page_desc = nullptr;
      m_rm->get_evict_manager()->send_work(w);
    }

    unlock();
  }

  // Return nullptr if page not present, PageDescriptor * otherwise
  PageDescriptor* Buffer::page_already_present( char* page_addr ) {
    while (1) {
      auto pp = m_present_pages.find(page_addr);
    
      // There is a chance that the state of this page is not/no-longer
      // PRESENT.  If this is the case, we need to wait for it to finish
      // with whatever is happening to it and then check again
      //
      if ( pp != m_present_pages.end() ) {
        if ( pp->second->state != PageDescriptor::State::PRESENT ) {
          await_state_change_notification( pp->second );
        }
        else {
          return pp->second;
        }
      }
      else {
        return nullptr;
      }
    }
  }

  PageDescriptor* Buffer::get_page_descriptor(char* vaddr, RegionDescriptor* rd) {
    while ( m_free_pages.size() == 0 )  {
      ++m_waits_for_avail_pd;
      m_stats.not_avail++;
      pthread_cond_wait(&m_avail_pd_cond, &m_mutex);
      --m_waits_for_avail_pd;
    }

    PageDescriptor* rval;

    rval = m_free_pages.back();
    m_free_pages.pop_back();

    rval->page = vaddr;
    rval->region = rd;
    rval->dirty = false;
    rval->deferred = false;
    rval->set_state_filling();

    m_stats.pages_inserted++;
    auto it = m_busy_pages.begin();
    m_busy_pages.insert(it, rval);

    return rval;
  }

  uint64_t Buffer::apply_int_percentage( int percentage, uint64_t item ) {
    uint64_t rval;

    if ( percentage < 0 || percentage > 100)
      UMAP_ERROR("Invalid percentage (" << percentage << ") given");

    if ( percentage == 0 || percentage == 100 ) {
      rval = item;
    }
    else {
      float f = (float)((float)percentage / (float)100.0);
      rval = f * item;
    }
    return rval;
  }

  void Buffer::lock() {
    int err;
    if ( (err = pthread_mutex_trylock(&m_mutex)) != 0 ) {
      if (err != EBUSY)
        UMAP_ERROR("pthread_mutex_trylock failed: " << strerror(err));

      if ( (err = pthread_mutex_lock(&m_mutex)) != 0 )
        UMAP_ERROR("pthread_mutex_lock failed: " << strerror(err));
      m_stats.lock_collision++;
    }
    m_stats.lock++;
  }

  void Buffer::unlock() {
    pthread_mutex_unlock(&m_mutex);
  }

  void Buffer::wait_for_page_state( PageDescriptor* pd, PageDescriptor::State st) {
    while ( pd->state != st ) {
      await_state_change_notification(pd);
    }
  }

  void Buffer::wakeup_page_state_waiters( PageDescriptor* pd ) {
      auto pp = m_pages_awaiting_state_change.find(pd->page);
      if ( pp != m_pages_awaiting_state_change.end() )
        pthread_cond_broadcast(&m_state_change_cond);
  }

  void Buffer::await_state_change_notification( PageDescriptor* pd ) {
    m_stats.waits++;
    ++m_waits_for_state_change;
    m_pages_awaiting_state_change[pd->page] = m_waits_for_state_change;
    pthread_cond_wait(&m_state_change_cond, &m_mutex);
    --m_waits_for_state_change;
    if (m_waits_for_state_change == 0)
      m_pages_awaiting_state_change.erase(pd->page);
  }

  std::ostream& operator<<(std::ostream& os, const Umap::Buffer* b) {
    if ( b != nullptr ) {
      os << "{ m_size: " << b->m_size
        << ", m_waits_for_avail_pd: " << b->m_waits_for_avail_pd
        << ", m_array: " << (void*)(b->m_array)
        << ", m_present_pages.size(): " << std::setw(2) << b->m_present_pages.size()
        << ", m_free_pages.size(): " << std::setw(2) << b->m_free_pages.size()
        << ", m_busy_pages.size(): " << std::setw(2) << b->m_busy_pages.size()
        << ", m_evict_low_water: " << std::setw(2) << b->m_evict_low_water
        << ", m_evict_high_water: " << std::setw(2) << b->m_evict_high_water
        << " }\n"
        ;
    }
    else {
      os << "{ nullptr }";
    }
    return os;
  }

  std::ostream& operator<<(std::ostream& os, const Umap::BufferStats& stats) {
    os << "Buffer Statisics:\n"
      << "   Pages Inserted: " << std::setw(12) << stats.pages_inserted<< "\n"
      << "    Pages Deleted: " << std::setw(12) << stats.pages_deleted<< "\n"
      << " Unavailable wait: " << std::setw(12) << stats.not_avail<< "\n"
      << "            Locks: " << std::setw(12) << stats.lock << "\n"
      << "  Lock collisions: " << std::setw(12) << stats.lock_collision << "\n"
      << "            waits: " << std::setw(12) << stats.waits;
    return os;
  }
} // end of namespace Umap
