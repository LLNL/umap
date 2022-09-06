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

#include <atomic>

#define QUEUE_LEN 32

namespace Umap {

template <typename T>
class WorkQueue {
  public:
    WorkQueue(int max_workers, int qid)
      :   m_max_waiting(max_workers)
        , m_waiting_workers(0)
        , m_idle_waiters(0)
    {
      heads = new std::atomic<int>[max_workers];
      for(int i=0;i<max_workers;i++) heads[i].store(0);
      tails = new std::atomic<int>[max_workers];
      for(int i=0;i<max_workers;i++) tails[i].store(0);
      m_queue = (T**) malloc(sizeof(T*)*max_workers);
      for(int i=0;i<max_workers;i++) m_queue[i] = (T*) malloc(sizeof(T)*QUEUE_LEN);
      queue_id = qid;
      next_worker=0;

      //pthread_mutex_init(&m_mutex, NULL);
      //pthread_cond_init(&m_cond, NULL);
      //pthread_cond_init(&m_idle_cond, NULL);
    }

    ~WorkQueue() {
      //pthread_mutex_destroy(&m_mutex);
      //pthread_cond_destroy(&m_cond);
      //pthread_cond_destroy(&m_idle_cond);
    }

    void enqueue(T item) {//only one thread can increment head

      while(1){
        int curr_worker = next_worker;
        next_worker = (next_worker+1) % m_max_waiting;

        int head_old = heads[curr_worker].load();
        int tail_old = tails[curr_worker].load();
        int size = (head_old<=tail_old) ?(tail_old-head_old) : (QUEUE_LEN - head_old + tail_old );

        if( size!=(QUEUE_LEN-1) ){         
          m_queue[curr_worker][tail_old] = item;
          int tail_new = (tail_old+1)%QUEUE_LEN;
          tails[curr_worker].store(tail_new);
#ifdef PROF
          printf("enqueue queue %d worker %d head_old %d tail_old %d tail_new %d\n", queue_id, curr_worker, head_old, tail_old, tail_new);
#endif           
          return;
        } 
      }
    }
    
    T dequeue(int tid=0) { //each worker thread can only increment its own tail   

      while(1) {
        int head_old = heads[tid].load();
        int tail_old = tails[tid].load();
        int size = (head_old<=tail_old) ?(tail_old-head_old) : (QUEUE_LEN - head_old + tail_old );

#ifdef PROF
        printf("dequeue queue_id %d curr_worker %d head_old %d tail_old %d \n", queue_id, tid, head_old, tail_old);
#endif      
        if( size>0 ){
          T item = m_queue[tid][head_old];
          int head_new = (head_old+1)%QUEUE_LEN ;
          heads[tid].store(head_new);
          return item;
        }
     }
    }

    void wait_for_idle( void ) {
      for(int i=0; i<m_max_waiting;i++){

#ifdef PROF        
        printf("wait_for_idle:: queue_id %d tid %d tail %d head %d\n", queue_id, i, tails[i].load(), heads[i].load() );
#endif

        while(1){
          int head_old = heads[i].load();
          int tail_old = tails[i].load();
          if(head_old==tail_old)
            break;
        }
      }
    }

    bool is_empty() {
      for(int i=0; i<m_max_waiting; i++ ){
        int head_old = heads[i].load();
        int tail_old = tails[i].load();
        if(head_old!=tail_old)
          return false;
      }
      return true;
    } 

    int get_queue_id() { return queue_id;}

  private:
    //pthread_mutex_t m_mutex;
    //pthread_cond_t m_cond;
    //pthread_cond_t m_idle_cond;
    //std::deque<T> m_queue;
    T **m_queue;
    int queue_id;
    int next_worker;
    std::atomic<int> *heads;
    std::atomic<int> *tails;
   
    uint64_t m_max_waiting;
    uint64_t m_waiting_workers;
    int m_idle_waiters;
};

} // end of namespace Umap

#endif // _UMAP_WorkQueue_HPP
