//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_WorkQueue_HPP
#define _UMAP_WorkQueue_HPP

#include <pthread.h>

#include "umap/Macros.hpp"

namespace Umap {

template <typename T>
class WorkQueue {
  public:
    WorkQueue() {
      pthread_mutex_init(&m_mutex, NULL);
      pthread_cond_init(&m_cond, NULL);
    }

    ~WorkQueue() {
      pthread_mutex_destroy(&m_mutex);
      pthread_cond_destroy(&cond_v);
    }

    enqueue(T item) {
      pthread_mutex_lock(&m_mutex);
      m_queue.push_back(item);
      pthread_cond_signal(&m_cond);
      pthread_mutex_unlock(&m_mutex);
    }

    T dequeue() {
      pthread_mutex_lock(&m_mutex);

      while ( m_queue.size() == 0 )
        pthread_cond_wait(&m_condv, &m_mutex);

      auto item = m_queue.front();
      m_queue.pop_front();

      pthread_mutex_unlock(&m_mutex);
      return item;
    }

    bool is_empty() {
      pthread_mutex_lock(&m_mutex);
      bool empty = (m_queue.size() == 0);
      pthread_mutex_unlock(&m_mutex);
      return empty;
    }

  private:
    pthread_mutex_t m_mutex;
    pthread_mutex_t m_cond;
    std::list<T> m_queue;
};

} // end of namespace Umap

#endif // _UMAP_WorkQueue_HPP
