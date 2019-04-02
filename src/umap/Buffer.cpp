//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <cassert>
#include <stdlib.h>

#include "umap/config.h"

#include "umap/Buffer.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {

  Buffer::Buffer( uint64_t size )
    :   m_size(size)
      , m_alloc_idx(0)
      , m_free_idx(0)
      , m_alloc_cnt(0)
  {
    m_array = (PageDescriptor *)calloc(m_size, sizeof(PageDescriptor));
    if ( m_array == nullptr ) {
      UMAP_ERROR("Failed to allocate " << m_size*sizeof(PageDescriptor)
          << " bytes for buffer page descriptors");
    }
  }

  Buffer::~Buffer( void )
  {
    assert(m_present_pages.size() == 0);
    assert(m_alloc_cnt == 0);
    free(m_array);
  }

  PageDescriptor* Buffer::allocate_page_descriptor( void* page )
  {
    if ( m_alloc_cnt < m_size ) {
      PageDescriptor* p = m_array + m_alloc_idx;
      m_alloc_idx = (m_alloc_idx + 1) % m_size;
      m_alloc_cnt++;
      p->set_page(page);
      m_present_pages[page] = p;
      UMAP_LOG(Debug, 
          p << " allocated for " << page 
          << ", free idx=" << m_free_idx 
          << " alloc idx=" << m_alloc_idx
          << " cnt=" << m_alloc_cnt);
      return p;
    }
    return nullptr;
  }

  void Buffer::deallocate_page_descriptor( void )
  {
    PageDescriptor* p = m_alloc_cnt ? m_array + m_free_idx : nullptr;

    if ( p != nullptr ) {
      UMAP_LOG(Debug, p 
          << " freed for " << p->get_page()
          << " free idx=" << m_free_idx
          << " alloc idx=" << m_alloc_idx
          << " cnt=" << m_alloc_cnt);

      m_free_idx = (m_free_idx + 1) % m_size;
      m_alloc_cnt--;
      m_present_pages.erase(p->get_page());
      p->set_page(nullptr);
    }
  }

  bool Buffer::pages_still_present( void )
  {
    return m_alloc_cnt != 0;
  }
} // end of namespace Umap
