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

#ifdef LOCK_OPT
#include <atomic>
static int queue_id_g=0;
#endif

namespace Umap {
template <typename T>
class WorkQueue {
  public:
    WorkQueue(int max_workers)
      :   m_max_waiting(max_workers)
        , m_waiting_workers(0)
        , m_idle_waiters(0)
    {
#ifdef LOCK_OPT
      head.store(1);
      tail.store(0);
      queue_id = queue_id_g;
      queue_id_g ++;
#endif      
      pthread_mutex_init(&m_mutex, NULL);
      pthread_cond_init(&m_cond, NULL);
      pthread_cond_init(&m_idle_cond, NULL);
    }

    ~WorkQueue() {
      pthread_mutex_destroy(&m_mutex);
      pthread_cond_destroy(&m_cond);
      pthread_cond_destroy(&m_idle_cond);
    }

#ifndef LOCK_OPT
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
#else
    void enqueue(T item) {//only a UFFD thread can increment head

      int tail_old = tail.load();
      while( tail_old == head ){
        tail_old = tail.load();
      }
      //UMAP_LOG(Info, "queue_id " << queue_id << " tail " << tail << ", head "<< head);
      m_queue[head] = item;
      int head_new = head==127 ?0 : (head+1);
      head.store(head_new);
      //UMAP_LOG(Info, "queue_id " << queue_id << " tail " << tail << ", head "<< head);
    }
    
    T dequeue() { //multiple worker threads can increment tail
      int head_old = head.load();
      int tail_old = tail.load();
      int tail_new = (tail_old==127) ?0 : (tail_old+1);
      T item = m_queue[tail_new];
      //UMAP_LOG(Info, "queue_id " << queue_id << " tail " << tail << ", head "<< head << ", tail_new "<< tail_new);
      while ( (tail_new==head_old) || !tail.compare_exchange_weak(tail_old, tail_new )){
        head_old = head.load();
        tail_new = (tail_old==127) ?0 : (tail_old+1);
        item = m_queue[tail_new];
      }
      //UMAP_LOG(Info, "queue_id " << queue_id << " tail " << tail << ", head "<< head);
      return item;
    }

    void wait_for_idle( void ) {
      int tail_old = tail.load();
      int tail_new = (tail_old==127) ?0 : (tail_old+1);

      while ( !head.compare_exchange_weak(tail_new, tail_new ) ){
        tail_old = tail.load();
        tail_new = (tail_old==127) ?0 : (tail_old+1);
      }
    }

    bool is_empty() {
      
      int tail_old = tail.load();
      int tail_new = (tail_old==127) ?0 : (tail_old+1);
      //UMAP_LOG(Info,  "queue_id " << queue_id << " head "<< head << " tail " << tail);

      if ( head.compare_exchange_weak(tail_new, tail_new ) ){
        return true;
      }
      return false;
    }    
#endif

  private:
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
    pthread_cond_t m_idle_cond;
    #ifndef LOCK_OPT
    std::list<T> m_queue;
    #else
    //std::deque<T> m_queue;
    T m_queue[128];
    int queue_id;
    std::atomic<int> head;
    std::atomic<int> tail;
    #endif
    uint64_t m_max_waiting;
    uint64_t m_waiting_workers;
    int m_idle_waiters;
};

} // end of namespace Umap

#endif // _UMAP_WorkQueue_HPP
