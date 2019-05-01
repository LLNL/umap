//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_Buffer_HPP
#define _UMAP_Buffer_HPP

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

#include "umap/PageDescriptor.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {
  struct BufferStats {
    BufferStats() :   lock_collision(0), lock(0), pages_inserted(0)
                    , pages_deleted(0), wait_for_present(0), not_avail(0)
    {};

    uint64_t lock_collision;
    uint64_t lock;
    uint64_t pages_inserted;
    uint64_t pages_deleted;
    uint64_t wait_for_present;
    uint64_t not_avail;
    uint64_t wait_on_oldest;
  };

  std::ostream& operator<<(std::ostream& os, const Umap::BufferStats& stats)
  {
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

  class Buffer {
    friend std::ostream& operator<<(std::ostream& os, const Umap::Buffer* b);
    friend std::ostream& operator<<(std::ostream& os, const Umap::BufferStats& stats);
    public:
      explicit Buffer(  uint64_t size, int low_water_threshold, int high_water_threshold)
        : m_size(size), m_fill_waiting_count(0), m_last_pd_waiting(nullptr)
      {
        m_array = (PageDescriptor *)calloc(m_size, sizeof(PageDescriptor));
        if ( m_array == nullptr )
          UMAP_ERROR("Failed to allocate " << m_size*sizeof(PageDescriptor)
              << " bytes for buffer page descriptors");

        for ( int i = 0; i < m_size; ++i ) {
          m_free_pages.push_back(&m_array[i]);
          m_array[i].m_state = Umap::PageDescriptor::State::FREE;
        }

        pthread_mutex_init(&m_mutex, NULL);
        pthread_cond_init(&m_available_descriptor_cond, NULL);
        pthread_cond_init(&m_oldest_page_ready_for_eviction, NULL);
        pthread_cond_init(&m_present_page_descriptor_cond, NULL);

        m_evict_low_water = apply_int_percentage(low_water_threshold, m_size);
        m_evict_high_water = apply_int_percentage(high_water_threshold, m_size);
      }

      ~Buffer( void )
      {
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
      void make_page_present(PageDescriptor* pd) {
        void* page_addr = pd->get_page_addr();

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
      void remove_page( PageDescriptor* pd ) {
        void* page_addr = pd->get_page_addr();

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
      bool evict_low_threshold_reached( void ) {
        return m_busy_pages.size() <= m_evict_low_water;
      }

      //
      // Called from Evict Manager to begin eviction process on oldest present
      // page
      //
      PageDescriptor* evict_oldest_page() {
        lock();

        if ( m_busy_pages.size() == 0 ) {
          unlock();
          return nullptr;
        }

        PageDescriptor* rval;

        rval = m_busy_pages.front();

        while ( rval->m_state != PageDescriptor::State::PRESENT ) {
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
      void process_page_event(
            void* paddr
          , bool iswrite
          , WorkerPool* fill_workers
          , WorkerPool* evict_workers
          , Store* store
      )
      {
        WorkItem work;

        lock();
        auto pd = page_already_present(paddr);

        if ( pd != nullptr ) {  // Page is already present
          if (iswrite && pd->page_is_dirty() == false) {
            work.page_desc = pd; work.store = nullptr;
            pd->mark_page_dirty();
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
          pd = get_page_descriptor(paddr);
          work.page_desc = pd; work.store = store;
          pd->set_state_filling();

          add_page(pd);

          if (iswrite)
            pd->mark_page_dirty();

          UMAP_LOG(Debug, "NEW: " << pd << " From: " << this);
        }

        fill_workers->send_work(work);

        //
        // Kick the eviction daemon if the high water mark has been reached
        //
        if ( m_busy_pages.size() == m_evict_high_water ) {
          WorkItem w;

          w.type = Umap::WorkItem::WorkType::THRESHOLD;
          w.page_desc = nullptr;
          w.store = nullptr;
          evict_workers->send_work(w);
        }

        unlock();
      }

    private:
      // Return nullptr if page not present, PageDescriptor * otherwise
      PageDescriptor* page_already_present( void* page_addr ) {
        while (1) {
          auto pp = m_present_pages.find(page_addr);
        
          // There is a chance that the state of this page is not/no-longer
          // PRESENT.  If this is the case, we need to wait for it to finish
          // with whatever is happening to it and then check again
          //
          if ( pp != m_present_pages.end() ) {
            if ( pp->second->m_state != PageDescriptor::State::PRESENT ) {
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

      void add_page( PageDescriptor* pd ) {
        m_present_pages[pd->get_page_addr()] = pd;
      }

      PageDescriptor* get_page_descriptor( void* page_addr ) {
        while ( m_free_pages.size() == 0 )  {
          ++m_fill_waiting_count;
          m_stats.not_avail++;
          pthread_cond_wait(&m_available_descriptor_cond, &m_mutex);
          --m_fill_waiting_count;
        }

        PageDescriptor* rval;

        rval = m_free_pages.back();
        m_free_pages.pop_back();

        rval->m_page = page_addr;
        rval->m_is_dirty = false;

        m_stats.pages_inserted++;
        m_busy_pages.push(rval);

        return rval;
      }

      void free_page_descriptor( PageDescriptor* pd ) {
        m_free_pages.push_back(pd);
      }

      uint64_t get_number_of_present_pages( void ) {
        return m_present_pages.size();
      }

      void lock() {
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

      void unlock() {
        pthread_mutex_unlock(&m_mutex);
      }

      uint64_t m_size;          // Maximum pages this buffer may have
      int m_fill_waiting_count; // # of IOs waiting to be filled
      PageDescriptor* m_last_pd_waiting;

      std::unordered_map<void*, bool> map_of_pages_awaiting_state_change;
      PageDescriptor* m_array;
      std::unordered_map<void*, PageDescriptor*> m_present_pages;
      std::vector<PageDescriptor*> m_free_pages;
      std::queue<PageDescriptor*> m_busy_pages;

      uint64_t m_evict_low_water;   // % to evict too
      uint64_t m_evict_high_water;  // % to start evicting

      pthread_mutex_t m_mutex;
      pthread_cond_t m_available_descriptor_cond;
      pthread_cond_t m_oldest_page_ready_for_eviction;
      pthread_cond_t m_present_page_descriptor_cond;

      BufferStats m_stats;

      uint64_t apply_int_percentage( int percentage, uint64_t item ) {
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

  };

  std::ostream& operator<<(std::ostream& os, const Umap::Buffer* b)
  {
    if ( b != nullptr ) {
      os << "{ m_size: " << b->m_size
        << ", m_fill_waiting_count: " << b->m_fill_waiting_count
        << ", m_array: " << (void*)(b->m_array)
        << ", m_present_pages.size(): " << std::setw(2) << b->m_present_pages.size()
        << ", m_free_pages.size(): " << std::setw(2) << b->m_free_pages.size()
        << ", m_busy_pages.size(): " << std::setw(2) << b->m_busy_pages.size()
        << ", m_evict_low_water: " << std::setw(2) << b->m_evict_low_water
        << ", m_evict_high_water: " << std::setw(2) << b->m_evict_high_water
        << " }";
    }
    else {
      os << "{ nullptr }";
    }

    return os;
  }
} // end of namespace Umap

#endif // _UMAP_Buffer_HPP
