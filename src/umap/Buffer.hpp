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

#include "umap/RegionDescriptor.hpp"
#include "umap/PageDescriptor.hpp"

namespace Umap {
  struct BufferStats {
    BufferStats() :   lock_collision(0), lock(0), pages_inserted(0)
                    , pages_deleted(0), wait_for_present(0), not_avail(0)
                    , wait_on_oldest(0)
    {};

    uint64_t lock_collision;
    uint64_t lock;
    uint64_t pages_inserted;
    uint64_t pages_deleted;
    uint64_t wait_for_present;
    uint64_t not_avail;
    uint64_t wait_on_oldest;
  };

  class Buffer {
    friend std::ostream& operator<<(std::ostream& os, const Umap::Buffer* b);
    friend std::ostream& operator<<(std::ostream& os, const Umap::BufferStats& stats);
    public:
      explicit Buffer( void );
      ~Buffer( void );

      void make_page_present(PageDescriptor* pd);
      void remove_page( PageDescriptor* pd );
      bool evict_low_threshold_reached( void );
      PageDescriptor* evict_oldest_page( void );
      void process_page_event(char* paddr, bool iswrite, RegionDescriptor* rd);

    private:
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

      PageDescriptor* page_already_present( void* page_addr );
      void add_page( PageDescriptor* pd );
      PageDescriptor* get_page_descriptor( void* page_addr, RegionDescriptor* rd );
      void free_page_descriptor( PageDescriptor* pd );
      uint64_t get_number_of_present_pages( void );
      void lock();
      void unlock();
      uint64_t apply_int_percentage( int percentage, uint64_t item );
  };

  std::ostream& operator<<(std::ostream& os, const Umap::BufferStats& stats);
  std::ostream& operator<<(std::ostream& os, const Umap::Buffer* b);

} // end of namespace Umap

#endif // _UMAP_Buffer_HPP
