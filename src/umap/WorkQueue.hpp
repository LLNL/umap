//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_WorkQueue_HPP
#define _UMAP_WorkQueue_HPP

#include <list>

#include <cstdint>
#include <pthread.h>
#include <unistd.h>

#include "umap/Uffd.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {
template <typename T>
class WorkQueue {
  public:
    WorkQueue(int max_workers)
      :   m_max_waiting(max_workers)
        , m_waiting_workers(0)
        , m_idle_waiters(0)
    {
      pthread_mutex_init(&m_mutex, NULL);
      pthread_cond_init(&m_cond, NULL);
      pthread_cond_init(&m_idle_cond, NULL);
    }

    ~WorkQueue() {
      pthread_mutex_destroy(&m_mutex);
      pthread_cond_destroy(&m_cond);
      pthread_cond_destroy(&m_idle_cond);
    }

    void enqueue(T item) {
      pthread_mutex_lock(&m_mutex);
      m_queue.push_back(item);
      pthread_cond_signal(&m_cond);
      pthread_mutex_unlock(&m_mutex);
    }

    T dequeue() {
      pthread_mutex_lock(&m_mutex);

      ++m_waiting_workers;

      while ( m_queue.size() == 0 ) {
        if (m_waiting_workers == m_max_waiting && m_idle_waiters)
          pthread_cond_signal(&m_idle_cond);

        pthread_cond_wait(&m_cond, &m_mutex);
      }

      --m_waiting_workers;

      auto item = m_queue.front();
      m_queue.pop_front();

      pthread_mutex_unlock(&m_mutex);
      return item;
    }

    void wait_for_idle( void ) {
      pthread_mutex_lock(&m_mutex);
      ++m_idle_waiters;

      while ( ! ( m_queue.size() == 0 && m_waiting_workers == m_max_waiting ) )
        pthread_cond_wait(&m_idle_cond, &m_mutex);

      --m_idle_waiters;
      pthread_mutex_unlock(&m_mutex);
    }

    bool is_empty() {
      pthread_mutex_lock(&m_mutex);
      bool empty = (m_queue.size() == 0);
      pthread_mutex_unlock(&m_mutex);
      return empty;
    }

  private:
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
    pthread_cond_t m_idle_cond;
    std::list<T> m_queue;
    uint64_t m_max_waiting;
    uint64_t m_waiting_workers;
    int m_idle_waiters;
};

} // end of namespace Umap

#endif // _UMAP_WorkQueue_HPP
