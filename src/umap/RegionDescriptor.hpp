//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
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
        , m_store(store) {
          ref_count=0;
	}

      ~RegionDescriptor( void ) {}

      inline uint64_t store_offset( char* addr ) {
        assert("Invalid address for calculating offset" && addr >= start() && addr < end());
        return (uint64_t)(addr - start());
      }

      inline uint64_t size( void )       { return m_umap_region_size;         }
      inline Store*   store( void )      { return m_store;                    }
      inline char*    start( void )      { return m_umap_region;              }
      inline uint64_t get_mmap_size()    { return m_mmap_region_size;         }
      inline char *   get_mmap_region()  { return m_mmap_region;              }
      inline char*    end( void )        { return start() + size();           }
      inline uint64_t count( void )      { return m_active_pages.size();      }
      inline void     acc_ref( void )    { ++ref_count;                       }
      inline void     rel_ref( void )    { --ref_count;                       }
      inline bool     can_release( void ){ return !ref_count;                 }

      inline void insert_page_descriptor(PageDescriptor* pd) {
        m_active_pages.insert(pd);
      }

      inline void erase_page_descriptor(PageDescriptor* pd) {
        UMAP_LOG(Debug, "Erasing PD: " << pd);
        m_active_pages.erase(pd);
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
      uint64_t ref_count;

      std::unordered_set<PageDescriptor*> m_active_pages;
  };
} // end of namespace Umap
#endif // _UMAP_RegionDescripto_HPP
