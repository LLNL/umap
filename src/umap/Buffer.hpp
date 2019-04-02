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
#include <time.h>
#include <unordered_map>
#include <vector>

#include "umap/util/Macros.hpp"

namespace Umap {
struct PageDescriptor {
  enum State {EMPTY = 0, FILLING = 1, PRESENT = 2, DIRTY = 3, FLUSHING = 4};
  void* page;
  State state;
};

class Buffer {
public:
  explicit Buffer( uint64_t size ) : m_size(size)
  {
    m_array = (PageDescriptor *)calloc(m_size, sizeof(PageDescriptor));
    if ( m_array == nullptr )
      UMAP_ERROR("Failed to allocate " << m_size*sizeof(PageDescriptor)
          << " bytes for buffer page descriptors");

    m_tail = &m_array[0];
    m_head = &m_array[0];

    pthread_mutex_init(&m_mutex, NULL);
    pthread_cond_init(&m_cond, NULL);
  }

  ~Buffer( void )
  {
    assert("Pages are still present" && m_present_pages.size() == 0);
    pthread_cond_destroy(&m_cond);
    pthread_mutex_destroy(&m_mutex);
    free(m_array);
  }

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

  inline PageDescriptor* add_page_to_buffer( void* )
  {
    return nullptr;
  }

  inline PageDescriptor* find_fill_buffer( void* )
  {
    return nullptr;
  }

  inline void set_page_present( PageDescriptor* )
  {
  }

  inline void set_page_dirty( PageDescriptor* )
  {
  }

  inline bool page_is_dirty( PageDescriptor* )
  {
    return false;
  }

  inline uint64_t size( void ) { 
    return m_size;
  }

  inline uint64_t count( void ) { 
    return ( ( m_tail > m_head )
      ? ((&m_array[m_size] - m_tail) + (m_head - &m_array[0]))
      : (m_head - m_tail) );
  }

private:
  PageDescriptor* m_array;  // Circular buffer &m_array[0] - &m_array[m_size]
  uint64_t m_size;          // Maximum pages this buffer may have
  PageDescriptor* m_tail;
  PageDescriptor* m_head;
  std::unordered_map<void*, PageDescriptor*> m_present_pages;
  pthread_mutex_t m_mutex;
  pthread_cond_t m_cond;
};
} // end of namespace Umap

#endif // _UMAP_Buffer_HPP
