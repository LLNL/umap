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

#include "umap/util/Macros.hpp"

namespace Umap {
  struct PageDescriptor {
    enum State { FREE, FILLING, PRESENT, UPDATING, LEAVING };
    void* m_page;
    bool m_is_dirty;
    State m_state;

    bool page_is_dirty() { return m_is_dirty; }
    void mark_page_dirty() { m_is_dirty = true; }
    void* get_page_addr() { return m_page; }

    std::string print_state() const
    {
      switch (m_state) {
        default:                                    return "???";
        case Umap::PageDescriptor::State::FREE:     return "FREE";
        case Umap::PageDescriptor::State::FILLING:  return "FILLING";
        case Umap::PageDescriptor::State::PRESENT:  return "PRESENT";
        case Umap::PageDescriptor::State::UPDATING: return "UPDATING";
        case Umap::PageDescriptor::State::LEAVING:  return "LEAVING";
      }
    }

    void set_state_free() {
      if ( m_state != LEAVING )
        UMAP_ERROR("Invalid state transition from: " << print_state());
      m_state = FREE;
    }

    void set_state_filling() {
      if ( m_state != FREE )
        UMAP_ERROR("Invalid state transition from: " << print_state());
      m_state = FILLING;
    }

    void set_state_present() {
      if ( m_state != FILLING && m_state != UPDATING )
        UMAP_ERROR("Invalid state transition from: " << print_state());
      m_state = PRESENT;
    }

    void set_state_updating() {
      if ( m_state != PRESENT )
        UMAP_ERROR("Invalid state transition from: " << print_state());
      m_state = UPDATING;
    }

    void set_state_leaving() {
      if ( m_state != PRESENT )
        UMAP_ERROR("Invalid state transition from: " << print_state());
      m_state = LEAVING;
    }
  };

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
      /** Buffer constructor
       * \param size Maximum number of pages in buffer
       * \param flush_threshold Integer percentage of Buffer capacify to be
       * reached before page flushers are activated.  If 0 or 100, the flushers
       * will only run when the Buffer is completely full.
       */
      explicit Buffer(  uint64_t size
                      , int low_water_threshold
                      , int high_water_threshold
               ) :   m_size(size)
                   , m_fill_waiting_count(0)
                   , m_last_pd_waiting(nullptr)
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

        m_flush_low_water = apply_int_percentage(low_water_threshold, m_size);
        m_flush_high_water = apply_int_percentage(high_water_threshold, m_size);
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

      bool flush_threshold_reached( void ) {
        return m_busy_pages.size() >= m_flush_high_water;
      }

      bool flush_low_threshold_reached( void ) {
        return m_busy_pages.size() <= m_flush_low_water;
      }

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

      void remove_page( PageDescriptor* pd ) {
        void* page_addr = pd->get_page_addr();

        m_present_pages.erase(page_addr);
        free_page_descriptor( pd );

        pd->set_state_free();

        if ( m_fill_waiting_count )
          pthread_cond_signal(&m_available_descriptor_cond);

        auto pp = map_of_pages_awaiting_state_change.find(page_addr);
        if ( pp != map_of_pages_awaiting_state_change.end() )
          pthread_cond_signal(&m_present_page_descriptor_cond);
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

      void make_page_present(PageDescriptor* pd) {
        void* page_addr = pd->get_page_addr();

        pd->set_state_present();

        if (m_last_pd_waiting == pd)
            pthread_cond_signal(&m_oldest_page_ready_for_eviction);

        auto pp = map_of_pages_awaiting_state_change.find(page_addr);
        if ( pp != map_of_pages_awaiting_state_change.end() )
          pthread_cond_signal(&m_present_page_descriptor_cond);
      }

      PageDescriptor* get_oldest_present_page_descriptor() {
        if ( m_busy_pages.size() == 0 )
          return nullptr;

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

    private:
      uint64_t m_size;          // Maximum pages this buffer may have
      int m_fill_waiting_count; // # of IOs waiting to be filled
      PageDescriptor* m_last_pd_waiting;

      std::unordered_map<void*, bool> map_of_pages_awaiting_state_change;
      PageDescriptor* m_array;
      std::unordered_map<void*, PageDescriptor*> m_present_pages;
      std::vector<PageDescriptor*> m_free_pages;
      std::queue<PageDescriptor*> m_busy_pages;

      uint64_t m_flush_low_water;   // % to flush too
      uint64_t m_flush_high_water;  // % to start flushing

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
        << ", m_flush_low_water: " << std::setw(2) << b->m_flush_low_water
        << ", m_flush_high_water: " << std::setw(2) << b->m_flush_high_water
        << " }";
    }
    else {
      os << "{ nullptr }";
    }

    return os;
  }

  std::ostream& operator<<(std::ostream& os, const Umap::PageDescriptor* pd)
  {
    if (pd != nullptr) {
      os << "{ m_page: " << (void*)(pd->m_page)
         << ", m_state: " << pd->print_state()
         << ", m_is_dirty: " << pd->m_is_dirty << " }";
    }
    else {
      os << "{ nullptr }";
    }
    return os;
  }
} // end of namespace Umap

#endif // _UMAP_Buffer_HPP
