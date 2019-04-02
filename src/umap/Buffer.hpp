//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_Buffer_HPP
#define _UMAP_Buffer_HPP

#include <unordered_map>
#include <mutex>

#include "umap/util/Macros.hpp"

namespace Umap {
  struct PageDescriptor {
    bool is_dirty() { return dirty; }
    void mark_dirty() { dirty = true; }
    void mark_clean() { dirty = false; }
    void* get_page() { return page; }
    void set_page(void* _page) { page = _page; mark_clean(); }
    void* page;
    bool dirty;
  };

  class Buffer {
    public:
      explicit Buffer( uint64_t size );
      ~Buffer( void );

      PageDescriptor* allocate_page_descriptor(void* page);
      void deallocate_page_descriptor( void );
      bool pages_still_present( void );

      PageDescriptor* present_page_descriptor(void* page);

      inline void lock( void ) { m_mutex.lock(); }
      inline void unlock( void ) { m_mutex.unlock(); }
      inline uint64_t size( void ) { 
        return ( ( m_alloc_idx > m_free_idx )
          ? ((m_size - m_alloc_idx) + m_free_idx)
          : (m_free_idx - m_alloc_idx) );
      }

    private:
      PageDescriptor* m_array;
      uint64_t m_size;          // # of elements in circular array
      uint64_t m_alloc_idx;
      uint64_t m_free_idx;
      uint64_t m_alloc_cnt;
      std::unordered_map<void*, PageDescriptor*> m_present_pages;
      std::mutex m_mutex;
  };
} // end of namespace Umap

#endif // _UMAP_Buffer_HPP
