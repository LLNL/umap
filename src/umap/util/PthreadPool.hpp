//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_Pthread_HPP
#define _UMAP_Pthread_HPP

#include <cstdint>
#include <pthread.h>
#include <vector>

#include "umap/util/Macros.hpp"

namespace Umap {
class PthreadPool {
  public:
    PthreadPool(uint64_t num_threads) : m_num_threads(num_threads), m_time_to_stop(false) {}
    virtual ~PthreadPool() {
      m_time_to_stop = true;
      for ( auto pt : m_threads ) {
        UMAP_LOG(Debug, "Stoping " << (void*)pt);
        (void) pthread_join(pt, NULL);
      }
    }

    void start_thread_pool() {
      UMAP_LOG(Debug, "Stoping Threads");
      pthread_t t;
      for ( uint64_t i = 0; i < m_num_threads; ++i) {
        if (pthread_create(&t, NULL, ThreadEntryFunc, this) != 0) {
          UMAP_ERROR("Failed to launch thread");
        }

        m_threads.push_back(t);
      }
    }

    bool time_to_stop_thread_pool() {
      return m_time_to_stop;
    }

  protected:
    virtual void ThreadEntry() = 0;

private:
  static void* ThreadEntryFunc(void * This) {
    ((PthreadPool *)This)->ThreadEntry();
    return NULL;
  }

  uint64_t m_num_threads;
  bool m_time_to_stop;
  std::vector<pthread_t> m_threads;
};
} // end of namespace Umap

#endif // _UMAP_PthreadPool_HPP
