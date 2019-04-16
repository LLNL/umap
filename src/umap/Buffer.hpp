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
#include <pthread.h>
#include <queue>
#include <time.h>
#include <unordered_map>
#include <vector>

#include "umap/util/Macros.hpp"

namespace Umap {
  struct PageDescriptor {
    enum State {EMPTY = 0, FILLING = 1, PRESENT = 2, DIRTY = 3, FLUSHING = 4};
    char* page;
    bool is_dirty;
    State state;
  };

  class Buffer {
    public:
      /** Buffer constructor
       * \param size Maximum number of pages in buffer
       * \param flush_threshold Integer percentage of Buffer capacify to be
       * reached before page flushers are activated.  If 0 or 100, the flushers
       * will only run when the Buffer is completely full.
       */
      explicit Buffer( uint64_t size, int low_water_threshold, int high_water_threshold ) : m_size(size) {
        m_array = (PageDescriptor *)calloc(m_size, sizeof(PageDescriptor));
        if ( m_array == nullptr )
          UMAP_ERROR("Failed to allocate " << m_size*sizeof(PageDescriptor)
              << " bytes for buffer page descriptors");

        for ( auto i = (m_size - m_size); i < m_size; ++i )
          m_free_pages.push_back(&m_array[i]);

        pthread_mutex_init(&m_mutex, NULL);
        pthread_cond_init(&m_cond, NULL);

        m_flush_low_water = apply_int_percentage(low_water_threshold, m_size);
        m_flush_high_water = apply_int_percentage(high_water_threshold, m_size);

      }

      ~Buffer( void )
      {
        assert("Pages are still present" && m_present_pages.size() == 0);
        pthread_cond_destroy(&m_cond);
        pthread_mutex_destroy(&m_mutex);
        free(m_array);
      }

#if 0
      std::vector<PageDescriptor*> set_pages_to_evict(
          uint64_t pages_in_buffer_threshold, time_t seconds_to_wait)
      {
        std::vector<PageDescriptor*> rval;

        int rc = 0;

        pthread_mutex_lock(&m_mutex);
        while (count() < pages_in_buffer_threshold && rc == 0) {
          UMAP_LOG(Debug, count() << " < " << pages_in_buffer_threshold
              << ".  Waiting for more pages");

          struct timespec ts;
          clock_gettime(CLOCK_REALTIME, &ts);
          ts.tv_sec += seconds_to_wait;
          rc = pthread_cond_timedwait(&m_cond, &m_mutex, &ts);
        }
        pthread_mutex_unlock(&m_mutex);

        if (rc == ETIMEDOUT)
          UMAP_LOG(Debug, "Timed out");

        return rval;
      }

      inline void remove_evicted_pages_from_buffer( void )
      {
      }

      inline PageDescriptor* add_page_to_buffer( void* ) { return nullptr; }

      inline PageDescriptor* find_fill_buffer( void* ) { return nullptr; }

      inline void set_page_present( PageDescriptor* )
      {
      }

      inline void set_page_dirty( PageDescriptor* )
      {
      }

      inline bool page_is_dirty(PageDescriptor* pd)
      {
        return pd->state == PageDescriptor::State::DIRTY;
      }

      inline uint64_t size( void ) {
        return m_size;
      }
#endif

    private:
      uint64_t m_size;          // Maximum pages this buffer may have

      PageDescriptor* m_array;
      std::unordered_map<void*, PageDescriptor*> m_present_pages;
      std::vector<PageDescriptor*> m_free_pages;
      std::queue<PageDescriptor*> m_busy_pages;

      uint64_t m_flush_low_water;   // % to flush too
      uint64_t m_flush_high_water;  // % to start flushing

      pthread_mutex_t m_mutex;
      pthread_cond_t m_cond;

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
} // end of namespace Umap
#endif // _UMAP_Buffer_HPP
