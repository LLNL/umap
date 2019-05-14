//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_RegionDescriptor_HPP
#define _UMAP_RegionDescriptor_HPP

#include <cstdint>
#include "umap/store/Store.hpp"

namespace Umap {
  class RegionDescriptor {
    public:
      RegionDescriptor(   char* umap_region, uint64_t umap_size
              , char* mmap_region, uint64_t mmap_size
              , Store* store ) :
          m_umap_region(umap_region), m_umap_region_size(umap_size)
        , m_mmap_region(mmap_region), m_mmap_region_size(mmap_size)
        , m_store(store) {}

      ~RegionDescriptor() {}

      char* get_region( void ) { return m_umap_region; }
      uint64_t get_size( void )   { return m_umap_region_size;   }

      uint64_t map_addr_to_region_offset( char* addr ) {
        return (uint64_t)(addr - m_umap_region);
      }

      char* get_end_of_region()   { return m_umap_region + m_umap_region_size; }
      Store*   get_store( void )  { return m_store;  }

    private:
      char*    m_umap_region;
      uint64_t m_umap_region_size;
      char*    m_mmap_region;
      uint64_t m_mmap_region_size;
      Store*   m_store;
  };
} // end of namespace Umap
#endif // _UMAP_RegionDescripto_HPP
