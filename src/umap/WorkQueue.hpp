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
      next_worker=0;
      heads = new std::atomic<int>[max_workers];
      for(int i=0;i<max_workers;i++) heads[i].store(0);
      tails = new std::atomic<int>[max_workers];
      for(int i=0;i<max_workers;i++) tails[i].store(0);
      m_queue = (T**) malloc(sizeof(T*)*max_workers);
      for(int i=0;i<max_workers;i++) m_queue[i] = (T*) malloc(sizeof(T)*8);
      queue_id = queue_id_g;
      queue_id_g ++;
      printf("queue_id %d max_workers %d \n", queue_id, max_workers);

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

      while(1){
        int curr_worker = next_worker;
        next_worker = ((next_worker+1)==m_max_waiting) ?0 :(next_worker+1);

        int head_old = heads[curr_worker].load();
        int tail_old = tails[curr_worker].load();
        int size = (head_old>=tail_old) ?(head_old-tail_old) : (8-tail_old+head_old);
        //if(queue_id==3) printf("108 curr_worker %d next_worker %d tail_old %d head_old %d size %d\n", curr_worker, next_worker, tail_old, head_old, size);
        if( size!=7 ){
          m_queue[curr_worker][head_old] = item;
          int head_new = head_old==7 ?0 : (head_old+1);
          heads[curr_worker].store(head_new);
          return;
        } 
      }
    }
    
    T dequeue(int tid=0) { //multiple worker threads can increment tail   

      while(1) {
        int head_old = heads[tid].load();
        int tail_old = tails[tid].load();
        int size = (head_old>=tail_old) ?(head_old-tail_old) : (8-tail_old+head_old);
        //if(queue_id==3) printf("130 tid %d tail_old %d head_old %d size %d\n", tid, tail_old, head_old, size);
      
        if( size>0 ){
          T item = m_queue[tid][tail_old];
          int tail_new = (tail_old==7) ?0 : (tail_old+1);
          tails[tid].store(tail_new);
          //if(queue_id==3) printf("136 tid %d size %d tail_old %d head_old %d tail_new %d paddr %p\n", tid, size, tail_old, head_old, tail_new, item.page_desc->page);
          return item;
        }
     }
    }

    void wait_for_idle( void ) {
      for(int i=0; i<m_max_waiting;i++){
        while(1){
          int head_old = heads[i].load();
          int tail_old = tails[i].load();
          int size = (head_old>=tail_old) ?(head_old-tail_old) : (8-tail_old+head_old);
          if(size==0)
            break;
        }
      }
    }

    bool is_empty() {
      bool result = true;
      for(int i=0; i<m_max_waiting;i++){
        int head_old = heads[i].load();
        int tail_old = tails[i].load();
        int size = (head_old>=tail_old) ?(head_old-tail_old) : (8-tail_old+head_old);
        if(size>0)
          return false;
      }
      return result;
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
    T **m_queue;
    int queue_id;
    int next_worker;
    std::atomic<int> *heads;
    std::atomic<int> *tails;
    #endif
    uint64_t m_max_waiting;
    uint64_t m_waiting_workers;
    int m_idle_waiters;
};

} // end of namespace Umap

#endif // _UMAP_WorkQueue_HPP
