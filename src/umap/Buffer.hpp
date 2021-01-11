//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_Buffer_HPP
#define _UMAP_Buffer_HPP

#include <pthread.h>
#include <unordered_map>
#include <vector>
#include <deque>

#include "umap/RegionDescriptor.hpp"
#include "umap/PageDescriptor.hpp"

namespace Umap {
  class RegionManager;

  struct BufferStats {
    BufferStats() :   lock_collision(0), lock(0), pages_inserted(0)
                    , pages_deleted(0), not_avail(0), waits(0)
                    , events_processed(0)
    {};

    uint64_t lock_collision;
    uint64_t lock;
    uint64_t pages_inserted;
    uint64_t pages_deleted;
    uint64_t not_avail;
    uint64_t waits;
    uint64_t events_processed;
  };

  class Buffer {
    friend std::ostream& operator<<(std::ostream& os, const Umap::Buffer* b);
    friend std::ostream& operator<<(std::ostream& os, const Umap::BufferStats& stats);
    public:
      void mark_page_as_present(PageDescriptor* pd);
      void mark_page_as_free( PageDescriptor* pd );

      bool low_threshold_reached( void );

      void fetch_and_pin(char* paddr, uint64_t size);

      PageDescriptor* evict_oldest_page( void );
      std::vector<PageDescriptor*> evict_oldest_pages( void );
      void process_page_event(char* paddr, bool iswrite, RegionDescriptor* rd);
      void evict_region(RegionDescriptor* rd);
      void flush_dirty_pages();
    
      explicit Buffer( void );
      ~Buffer( void );

    private:
      RegionManager& m_rm;
      uint64_t m_size;          // Maximum pages this buffer may have
      PageDescriptor* m_array;

      std::unordered_map<char*, PageDescriptor*> m_present_pages;

      std::vector<PageDescriptor*> m_free_pages;
      std::deque<PageDescriptor*> m_busy_pages;

      uint64_t m_evict_low_water;   // % to evict too
      uint64_t m_evict_high_water;  // % to start evicting

      pthread_mutex_t m_mutex;

      int m_waits_for_avail_pd;
      pthread_cond_t m_avail_pd_cond;

      int m_waits_for_state_change;
      pthread_cond_t m_state_change_cond;

      BufferStats m_stats;
      bool is_monitor_on;
      pthread_t monitorThread;
      void monitor(void);
      static void* MonitorThreadEntryFunc(void * obj){
	      ((Buffer *) obj)->monitor();
        return NULL;
      }

      void release_page_descriptor( PageDescriptor* pd );

      PageDescriptor* page_already_present( char* page_addr );
      PageDescriptor* get_page_descriptor( char* page_addr, RegionDescriptor* rd );
      uint64_t apply_int_percentage( int percentage, uint64_t item );

      void lock();
      void unlock();
      void wait_for_page_state( PageDescriptor* pd, PageDescriptor::State st);
  };

  std::ostream& operator<<(std::ostream& os, const Umap::BufferStats& stats);
  std::ostream& operator<<(std::ostream& os, const Umap::Buffer* b);

} // end of namespace Umap

#endif // _UMAP_Buffer_HPP
