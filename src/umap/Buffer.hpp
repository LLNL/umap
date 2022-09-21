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
#include <atomic>

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

      inline bool low_threshold_reached( void ) {return (m_busy_page_size < m_evict_low_water) ;}
      void process_page_events(RegionDescriptor* rd, char** paddrs, bool *iswrites, int num_pages);


      void fetch_and_pin(char* paddr, uint64_t size);
      void adjust_hi_lo_watermark();

      std::vector<PageDescriptor*> evict_oldest_pages( void );
      void evict_region(RegionDescriptor* rd);
      void flush_dirty_pages();
      size_t get_busy_pages(){return m_busy_pages.size(); }
    
      explicit Buffer( RegionManager& rm );
      ~Buffer( void );

    private:
      RegionManager& m_rm;
      PageDescriptor* m_array;

      //std::unordered_map<char*, PageDescriptor*> m_present_pages;

      std::vector<PageDescriptor*> m_free_pages;
      std::vector<PageDescriptor*> m_free_pages_secondary; // evictors append to this to avoid lock on the free_list
      std::deque<PageDescriptor*>  m_busy_pages;
      uint64_t m_free_page_size;  // the number of free pages in the unit of umap psize
      std::atomic<uint64_t> m_free_page_secondary_size;
      uint64_t m_busy_page_size;  // the number of busy pages in the unit of umap psize

      uint64_t m_evict_low_water;   // % to stop  evicting
      uint64_t m_evict_high_water;  // % to start evicting
      uint64_t m_psize; // % global umap page size, regional page size must be a multiple of m_psize

      pthread_mutex_t m_mutex;
      pthread_mutex_t m_free_pages_secondary_mutex;

      int m_waits_for_avail_pd;
      pthread_cond_t m_avail_pd_cond;

      int m_waits_for_present_state_change;
      int m_waits_for_free_state_change;
      pthread_cond_t m_present_state_change_cond;
      pthread_cond_t m_free_state_change_cond;

      BufferStats m_stats;
      bool is_monitor_on;
      pthread_t monitorThread;
      void monitor(void);
      static void* MonitorThreadEntryFunc(void * obj){
	      ((Buffer *) obj)->monitor();
        return NULL;
      }

      //PageDescriptor* page_already_present( char* page_addr );
      //PageDescriptor* get_page_descriptor( char* page_addr, RegionDescriptor* rd );
      uint64_t apply_int_percentage( int percentage, uint64_t item );

      void lock();
      void unlock();
      void wait_for_page_present_state( PageDescriptor* pd);
  };

  std::ostream& operator<<(std::ostream& os, const Umap::BufferStats& stats);
  std::ostream& operator<<(std::ostream& os, const Umap::Buffer* b);

} // end of namespace Umap

#endif // _UMAP_Buffer_HPP
