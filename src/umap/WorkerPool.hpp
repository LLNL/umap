//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_Pthread_HPP
#define _UMAP_Pthread_HPP

#include <cstdint>
#include <pthread.h>
#include <string>
#include <vector>

#include "umap/PageDescriptor.hpp"
#include "umap/WorkQueue.hpp"
#include "umap/util/Macros.hpp"



namespace Umap {
  struct WorkItem {
    enum WorkType { NONE, EXIT, THRESHOLD, EVICT, FLUSH };
    PageDescriptor* page_desc;
    WorkType type;
    #ifdef PROF
    std::chrono::steady_clock::time_point timing;
    #endif
  };

  static std::ostream& operator<<(std::ostream& os, const Umap::WorkItem& b)
  {
    os << "{ page_desc: " << b.page_desc;

    switch (b.type) {
      default: os << ", type: Unknown(" << b.type << ")"; break;
      case Umap::WorkItem::WorkType::NONE: os << ", type: " << "NONE"; break;
      case Umap::WorkItem::WorkType::EXIT: os << ", type: " << "EXIT"; break;
      case Umap::WorkItem::WorkType::THRESHOLD: os << ", type: " << "THRESHOLD"; break;
      case Umap::WorkItem::WorkType::EVICT: os << ", type: " << "EVICT"; break;
      case Umap::WorkItem::WorkType::FLUSH: os << ", type: " << "FLUSH"; break;
    }

    os << " }";
    return os;
  }
  
  class WorkerPool {
    public:
      WorkerPool(const std::string& pool_name, uint64_t num_threads)
        :   m_pool_name(pool_name)
          , m_num_threads(num_threads)
          , m_wq(new WorkQueue<WorkItem>(num_threads, queue_id_g))
      {
        if (m_pool_name.length() > 15)
          m_pool_name.resize(15);
        
        queue_id_g++;
      }

      virtual ~WorkerPool() {
        stop_thread_pool();
        delete m_wq;
      }

      inline void send_work(const WorkItem& work) {
        m_wq->enqueue(work);
      }

      inline WorkItem get_work(int t_id=0) {
        return m_wq->dequeue(t_id);
      }

      bool wq_is_empty( void ) {
        return m_wq->is_empty();
      }

      void start_thread_pool() {

        UMAP_LOG(Debug, m_pool_name << " of " << m_num_threads << " threads on queue " << m_wq->get_queue_id() );

        for ( uint64_t i = 0; i < m_num_threads; ++i) {
          pthread_t t;

          if (pthread_create(&t, NULL, ThreadEntryFunc, this) != 0)
            UMAP_ERROR("Failed to launch thread");

          if (pthread_setname_np(t, m_pool_name.c_str()) != 0)
            UMAP_ERROR("Failed to set thread name");

          m_threads.push_back(t);
        }
      }

      void stop_thread_pool() {
        WorkItem w = {.page_desc = nullptr, .type = Umap::WorkItem::WorkType::EXIT };

        //
        // This will inform all of the threads it is time to go away
        //
        for ( uint64_t i = 0; i < m_num_threads; ++i)
          send_work(w);

        //
        // Wait for all of the threads to exit
        //
        for ( auto pt : m_threads )
          (void) pthread_join(pt, NULL);

        m_threads.clear();
      }

      void wait_for_idle( void ) {
        m_wq->wait_for_idle();
      }

    protected:
      virtual void ThreadEntry() = 0;

    private:
      static void* ThreadEntryFunc(void * This) {
        ((WorkerPool *)This)->ThreadEntry();
        return NULL;
      }

      std::string             m_pool_name;
      uint64_t                m_num_threads;
      WorkQueue<WorkItem>*    m_wq;
      std::vector<pthread_t>  m_threads;
      static int queue_id_g;
  };

} // end of namespace Umap



#endif // _UMAP_WorkerPool_HPP
