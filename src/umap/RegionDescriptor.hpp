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
                        , Store* store )
        : m_umap_region(umap_region), m_umap_region_size(umap_size)
        , m_mmap_region(mmap_region), m_mmap_region_size(mmap_size)
        , m_store(store) {}

      ~RegionDescriptor( void ) {}

      inline uint64_t store_offset( char* addr ) {
        assert("Invalid address for calculating offset" && addr >= start() && addr < end());
        return (uint64_t)(addr - start());
      }

      inline uint64_t size( void )     { return m_umap_region_size;         }
      inline Store*   store( void )    { return m_store;                    }
      inline char*    start( void )    { return m_umap_region;              }
      inline char*    end( void )      { return start() + size();           }
      inline uint64_t count( void )    { return m_active_pages.size();      }

      inline void insert_page_descriptor(PageDescriptor* pd) {
        #ifndef LOCK_OPT
        m_active_pages.insert(pd);
        #else
        m_present_pages[pd->page] = pd;
        //UMAP_LOG(Info, "inserted: " << pd << " m_present_pages.size()=" << m_present_pages.size());  
        //for(auto it : m_present_pages)
          //UMAP_LOG(Info, " : " << m_present_pages.size() << " : "<< it.second );
        #endif
      }
      #ifdef LOCK_OPT
      inline PageDescriptor* find(char* paddr) {
        
        //for(auto it : m_present_pages)
          //UMAP_LOG(Info, " : " << it.second );
        auto it = m_present_pages.find(paddr);
        if( it==m_present_pages.end() )
          return nullptr;
        else
          return it->second;
      }
      #endif       

      inline void erase_page_descriptor(PageDescriptor* pd) {
        #ifndef LOCK_OPT
        UMAP_LOG(Debug, "Erasing PD: " << pd);
        m_active_pages.erase(pd);
        #else
        m_present_pages.erase(pd->page);
        #endif        
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

    private:
      char*    m_umap_region;
      uint64_t m_umap_region_size;
      char*    m_mmap_region;
      uint64_t m_mmap_region_size;
      Store*   m_store;

      std::unordered_set<PageDescriptor*> m_active_pages;
      #ifdef LOCK_OPT
      std::unordered_map<char*, PageDescriptor*> m_present_pages;
      #endif
  };
} // end of namespace Umap
#endif // _UMAP_RegionDescripto_HPP
