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
#include <pthread.h>
#include <queue>
#include <time.h>
#include <unordered_map>
#include <vector>

#include "umap/util/Macros.hpp"

namespace Umap {
  struct PageDescriptor {
    void* m_page;
    bool m_is_dirty;

    bool page_is_dirty() { return m_is_dirty; }
    void mark_page_dirty() { m_is_dirty = true; }
    void set_page_addr(void* paddr) { m_page = paddr; }
    void* get_page_addr() { return m_page; }
  };

  class Buffer {
    friend std::ostream& operator<<(std::ostream& os, const Umap::Buffer* b);
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

        for ( int i = 0; i < m_size; ++i )
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

      //
      // Course grain lock against entire buffer.  We may need to make this
      // finer grained later if needed
      //
      void lock() {
        pthread_mutex_lock(&m_mutex);
      }

      void unlock() {
        pthread_mutex_unlock(&m_mutex);
      }

      // Return nullptr if page not present, PageDescriptor * otherwise
      PageDescriptor* page_already_present( void* page_addr ) {
        auto pp = m_present_pages.find(page_addr);
        if ( pp != m_present_pages.end() )
          return pp->second;
        else
          return nullptr;
      }

      void mark_page_present( PageDescriptor* pd ) {
        m_present_pages[pd->get_page_addr()] = pd;
      }

      PageDescriptor* get_page_descriptor( void* page_addr ) {
        PageDescriptor* rval;
        UMAP_LOG(Debug, this);

        while ( m_free_pages.size() == 0 )
          pthread_cond_wait(&m_cond, &m_mutex);

        rval = m_free_pages.back();
        m_free_pages.pop_back();

        rval->m_page = page_addr;
        rval->m_is_dirty = false;

        m_busy_pages.push(rval);

        return rval;
      }

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

  std::ostream& operator<<(std::ostream& os, const Umap::Buffer* b)
  {
    os << "{ m_size: " << b->m_size
      << ", m_array: " << (void*)(b->m_array)
      << ", m_present_pages.size(): " << b->m_present_pages.size()
      << ", m_free_pages.size(): " << b->m_free_pages.size()
      << ", m_busy_pages.size(): " << b->m_busy_pages.size()
      << ", m_flush_low_water: " << b->m_flush_low_water
      << ", m_flush_high_water: " << b->m_flush_high_water
      << " }";

    return os;
  }

  std::ostream& operator<<(std::ostream& os, const Umap::PageDescriptor* pd)
  {
    os << "{ m_page: " << (void*)(pd->m_page) << ", m_is_dirty: " << pd->m_is_dirty << " }";
    return os;
  }
} // end of namespace Umap

#endif // _UMAP_Buffer_HPP
