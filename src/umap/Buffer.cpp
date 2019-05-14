//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include "umap/config.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <pthread.h>
#include <queue>
#include <time.h>
#include <unordered_map>
#include <vector>

#include "umap/Buffer.hpp"
#include "umap/FillWorkers.hpp"
#include "umap/PageDescriptor.hpp"
#include "umap/RegionManager.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {
  Buffer::Buffer( void )
    :     m_size(RegionManager::getInstance()->get_max_pages_in_buffer())
        , m_fill_waiting_count(0)
        , m_last_pd_waiting(nullptr)
  {
    m_array = (PageDescriptor *)calloc(m_size, sizeof(PageDescriptor));
    if ( m_array == nullptr )
      UMAP_ERROR("Failed to allocate " << m_size*sizeof(PageDescriptor)
          << " bytes for buffer page descriptors");

    for ( int i = 0; i < m_size; ++i ) {
      m_free_pages.push_back(&m_array[i]);
      m_array[i].state = Umap::PageDescriptor::State::FREE;
    }

    pthread_mutex_init(&m_mutex, NULL);
    pthread_cond_init(&m_available_descriptor_cond, NULL);
    pthread_cond_init(&m_oldest_page_ready_for_eviction, NULL);
    pthread_cond_init(&m_present_page_descriptor_cond, NULL);

    m_evict_low_water = apply_int_percentage(RegionManager::getInstance()->get_evict_low_water_threshold(), m_size);
    m_evict_high_water = apply_int_percentage(RegionManager::getInstance()->get_evict_high_water_threshold(), m_size);
  }

  Buffer::~Buffer( void ) {
    std::cout << m_stats << std::endl;

    assert("Pages are still present" && m_present_pages.size() == 0);
    pthread_cond_destroy(&m_available_descriptor_cond);
    pthread_cond_destroy(&m_oldest_page_ready_for_eviction);
    pthread_cond_destroy(&m_present_page_descriptor_cond);
    pthread_mutex_destroy(&m_mutex);
    free(m_array);
  }

  //
  // Called from FillWorker threads after IO has completed
  //
  void Buffer::make_page_present(PageDescriptor* pd) {
    void* page_addr = pd->page;

    lock();

    pd->set_state_present();

    if (m_last_pd_waiting == pd)
        pthread_cond_signal(&m_oldest_page_ready_for_eviction);

    auto pp = map_of_pages_awaiting_state_change.find(page_addr);
    if ( pp != map_of_pages_awaiting_state_change.end() )
      pthread_cond_broadcast(&m_present_page_descriptor_cond);
    
    unlock();
  }

  //
  // Called from EvictWorkers to remove page from active list and place
  // onto the free list.
  //
  void Buffer::remove_page( PageDescriptor* pd ) {
    void* page_addr = pd->page;

    lock();
    m_present_pages.erase(page_addr);
    free_page_descriptor( pd );

    pd->set_state_free();

    if ( m_fill_waiting_count )
      pthread_cond_signal(&m_available_descriptor_cond);

    auto pp = map_of_pages_awaiting_state_change.find(page_addr);
    if ( pp != map_of_pages_awaiting_state_change.end() )
      pthread_cond_broadcast(&m_present_page_descriptor_cond);
    unlock();
  }

  //
  // Called by Evict Manager to determine when to stop evicting (no lock)
  //
  bool Buffer::evict_low_threshold_reached( void ) {
    return m_busy_pages.size() <= m_evict_low_water;
  }

  //
  // Called from Evict Manager to begin eviction process on oldest present
  // page
  //
  PageDescriptor* Buffer::evict_oldest_page() {
    lock();

    if ( m_busy_pages.size() == 0 ) {
      unlock();
      return nullptr;
    }

    PageDescriptor* rval;

    rval = m_busy_pages.front();

    while ( rval->state != PageDescriptor::State::PRESENT ) {
      m_stats.wait_on_oldest++;
      m_last_pd_waiting = rval;
      pthread_cond_wait(&m_oldest_page_ready_for_eviction, &m_mutex);
    }
    m_last_pd_waiting = nullptr;

    m_busy_pages.pop();
    m_stats.pages_deleted++;

    rval->set_state_leaving();

    unlock();
    return rval;
  }

  //
  // Called from Fill Manager
  //
  void Buffer::process_page_event(char* paddr, bool iswrite, RegionDescriptor* rd)
  {
    WorkItem work;

    lock();
    auto pd = page_already_present((void*)paddr);

    if ( pd != nullptr ) {  // Page is already present
      if (iswrite && pd->is_dirty == false) {
        work.page_desc = pd; work.store = nullptr;
        pd->is_dirty = true;
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
      pd = get_page_descriptor((void*)paddr, rd);
      work.page_desc = pd;
      work.store = rd->get_store();
      work.offset = rd->map_addr_to_region_offset(paddr);

      add_page(pd);

      if (iswrite)
        pd->is_dirty = true;

      UMAP_LOG(Debug, "NEW: " << pd << " From: " << this);
    }

    RegionManager::getInstance()->get_fill_workers_h()->send_work(work);

    //
    // Kick the eviction daemon if the high water mark has been reached
    //
    if ( m_busy_pages.size() == m_evict_high_water ) {
      WorkItem w;

      w.type = Umap::WorkItem::WorkType::THRESHOLD;
      w.page_desc = nullptr;
      w.store = nullptr;
      RegionManager::getInstance()->get_evict_manager()->send_work(w);
    }

    unlock();
  }

  // Return nullptr if page not present, PageDescriptor * otherwise
  PageDescriptor* Buffer::page_already_present( void* page_addr ) {
    while (1) {
      auto pp = m_present_pages.find(page_addr);
    
      // There is a chance that the state of this page is not/no-longer
      // PRESENT.  If this is the case, we need to wait for it to finish
      // with whatever is happening to it and then check again
      //
      if ( pp != m_present_pages.end() ) {
        if ( pp->second->state != PageDescriptor::State::PRESENT ) {
          m_stats.wait_for_present++;
          map_of_pages_awaiting_state_change[page_addr];
          pthread_cond_wait(&m_present_page_descriptor_cond, &m_mutex);
          map_of_pages_awaiting_state_change.erase(page_addr);
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

  void Buffer::add_page( PageDescriptor* pd ) {
    m_present_pages[pd->page] = pd;
  }

  PageDescriptor* Buffer::get_page_descriptor(void* vaddr, RegionDescriptor* rd) {
    while ( m_free_pages.size() == 0 )  {
      ++m_fill_waiting_count;
      m_stats.not_avail++;
      pthread_cond_wait(&m_available_descriptor_cond, &m_mutex);
      --m_fill_waiting_count;
    }

    PageDescriptor* rval;

    rval = m_free_pages.back();
    m_free_pages.pop_back();

    rval->page = vaddr;
    rval->region = rd;
    rval->is_dirty = false;
    rval->set_state_filling();

    m_stats.pages_inserted++;
    m_busy_pages.push(rval);

    return rval;
  }

  void Buffer::free_page_descriptor( PageDescriptor* pd ) {
    m_free_pages.push_back(pd);
  }

  uint64_t Buffer::get_number_of_present_pages( void ) {
    return m_present_pages.size();
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

  std::ostream& operator<<(std::ostream& os, const Umap::Buffer* b) {
    if ( b != nullptr ) {
      os << "{ m_size: " << b->m_size
        << ", m_fill_waiting_count: " << b->m_fill_waiting_count
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
      << "   Presence waits: " << std::setw(12) << stats.wait_for_present<< "\n"
      << "     Oldest waits: " << std::setw(12) << stats.wait_on_oldest<< "\n"
      << " Unavailable wait: " << std::setw(12) << stats.not_avail<< "\n"
      << "            Locks: " << std::setw(12) << stats.lock << "\n"
      << "  Lock collisions: " << std::setw(12) << stats.lock_collision;
    return os;
  }
} // end of namespace Umap
