//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_RegionDescriptor_HPP
#define _UMAP_RegionDescriptor_HPP

#include <cstdint>
#include <pthread.h>
#include <string.h>
#include <unordered_set>

#include "umap/PageDescriptor.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {
  class RegionDescriptor {
    public:
      RegionDescriptor(   char* umap_region, uint64_t umap_size
                        , char* mmap_region, uint64_t mmap_size
                        , Store* store )
        : m_umap_region(umap_region), m_umap_region_size(umap_size)
        , m_mmap_region(mmap_region), m_mmap_region_size(mmap_size)
        , m_store(store)
        , m_evict_count(0)
      {
        pthread_mutex_init(&m_mutex, NULL);
        pthread_cond_init(&m_pages_evicted_cond, NULL);
      }

      ~RegionDescriptor( void ) {
        pthread_cond_destroy(&m_pages_evicted_cond);
        pthread_mutex_destroy(&m_mutex);
      }

      inline uint64_t store_offset( char* addr )
                                       { return (uint64_t)(addr - start()); }
      inline uint64_t size( void )     { return m_umap_region_size;         }
      inline Store*   store( void )    { return m_store;                    }
      inline char*    start( void )    { return m_umap_region;              }
      inline char*    end( void )      { return start() + size();           }
      inline uint64_t count( void )    { return m_active_pages.size();      }

      inline void insert_page_descriptor(PageDescriptor* pd) {
        m_active_pages.insert(pd);
      }

      inline void erase_page_descriptor(PageDescriptor* pd) {
        m_active_pages.erase(pd);
        if (pd->deferred) {
          decrement_evict_count();
        }
      }

      inline PageDescriptor* get_next_page_descriptor( void ) {
        if ( m_active_pages.size() == 0 )
          return nullptr;

        auto it = m_active_pages.begin();
        auto rval = *it;
        rval->deferred = false;
        erase_page_descriptor(rval);

        return rval;
      }

      inline void wait_for_eviction_completion( void ) {
        int err;

        if ( (err = pthread_mutex_lock(&m_mutex)) != 0 )
          UMAP_ERROR("pthread_mutex_lock failed: " << strerror(err));

        while ( m_evict_count != 0 ) {
          UMAP_LOG(Debug, "evict_count: " << m_evict_count);
          pthread_cond_wait(&m_pages_evicted_cond, &m_mutex);
        }

        pthread_mutex_unlock(&m_mutex);
      }

      inline void     set_evict_count( void ) {
        m_evict_count = m_active_pages.size();
      }

      inline void     increment_evict_count( void ) {
        ++m_evict_count;
      }

      inline void decrement_evict_count( void ) {
        int err;

        if ( (err = pthread_mutex_lock(&m_mutex)) != 0 )
          UMAP_ERROR("pthread_mutex_lock failed: " << strerror(err));

        --m_evict_count;
        UMAP_LOG(Debug, "evict_count: " << m_evict_count);
        if ( m_evict_count == 0 )
          pthread_cond_signal(&m_pages_evicted_cond);

        pthread_mutex_unlock(&m_mutex);
      }

    private:
      char*    m_umap_region;
      uint64_t m_umap_region_size;
      char*    m_mmap_region;
      uint64_t m_mmap_region_size;
      Store*   m_store;
      uint64_t m_evict_count;

      pthread_mutex_t m_mutex;
      pthread_cond_t  m_pages_evicted_cond;

      std::unordered_set<PageDescriptor*> m_active_pages;
  };
} // end of namespace Umap
#endif // _UMAP_RegionDescripto_HPP
