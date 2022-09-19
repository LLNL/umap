//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_RegionDescriptor_HPP
#define _UMAP_RegionDescriptor_HPP

#include <cassert>
#include <cstdint>
#include <pthread.h>
#include <string.h>
#include <unordered_set>
#include <unordered_map>

#include "umap/PageDescriptor.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {
  class RegionDescriptor {
    public:
      RegionDescriptor(   char* umap_region, uint64_t umap_size
                        , char* mmap_region, uint64_t mmap_size
                        , Store* store
                        , uint64_t region_psize)
        : m_umap_region(umap_region), m_umap_region_size(umap_size)
        , m_mmap_region(mmap_region), m_mmap_region_size(mmap_size)
        , m_store(store)
        , m_region_psize(region_psize) {
          pthread_mutex_init(&m_mutex, NULL);
        }

      ~RegionDescriptor( void ) {
        pthread_mutex_destroy(&m_mutex);
      }

      inline uint64_t store_offset( char* addr ) {
        assert("Invalid address for calculating offset" && addr >= start() && addr < end());
        return (uint64_t)(addr - start());
      }

      inline uint64_t size( void )     { return m_umap_region_size;         }
      inline Store*   store( void )    { return m_store;                    }
      inline char*    start( void )    { return m_umap_region;              }
      inline char*    end( void )      { return start() + size();           }
      inline uint64_t count( void )    { return m_present_pages.size();      }
      inline uint64_t page_size( void ){ return m_region_psize; }
      void set_page_size( uint64_t p ) { m_region_psize = p; }

      inline void insert_page_descriptor(PageDescriptor* pd) {
        pthread_mutex_lock(&m_mutex);
        m_present_pages[pd->page] = pd;
        pthread_mutex_unlock(&m_mutex);
      } 

      inline PageDescriptor* find(char* paddr) {
        pthread_mutex_lock(&m_mutex);
        auto it = m_present_pages.find(paddr);
        pthread_mutex_unlock(&m_mutex);
        //
        // Most likely case
        //
        if( it==m_present_pages.end() )
          return nullptr;
        else{
          //
          // Next most likely is that it is in the buffer but may be in PRESENT, LEAVING, DEFERRED status
          //
          return it->second;
        }
      }
     

      void erase_page_descriptor(PageDescriptor* pd) {
        pthread_mutex_lock(&m_mutex);
        m_present_pages.erase(pd->page);
        pd->set_state_free();
        pd->page = nullptr;
        pthread_mutex_unlock(&m_mutex);
      }

      std::unordered_map<char*, PageDescriptor*> get_present_pages( void ) {
        pthread_mutex_lock(&m_mutex);
        std::unordered_map<char*, PageDescriptor*> res = m_present_pages;
        pthread_mutex_unlock(&m_mutex);
        return res;
      }

      bool has_present_pages( void ) {
        pthread_mutex_lock(&m_mutex);
        bool res = (m_present_pages.size()>0);
        pthread_mutex_unlock(&m_mutex);
        return res;
      }
            
      void print_present_pages( void ) {
        pthread_mutex_lock(&m_mutex);
        for(auto pd : m_present_pages )
          UMAP_LOG(Info, pd.second);
        pthread_mutex_unlock(&m_mutex);
      }

    private:
      char*    m_umap_region;
      uint64_t m_umap_region_size;
      char*    m_mmap_region;
      uint64_t m_mmap_region_size;
      Store*   m_store;
      uint64_t m_region_psize;
      pthread_mutex_t m_mutex;

      std::unordered_map<char*, PageDescriptor*> m_present_pages;
  };
} // end of namespace Umap
#endif // _UMAP_RegionDescripto_HPP
