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
    enum WorkType { NONE, EXIT, THRESHOLD, EVICT, FAST_EVICT, FLUSH };
    PageDescriptor* page_desc;
    WorkType type;
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
      case Umap::WorkItem::WorkType::FAST_EVICT: os << ", type: " << "FAST_EVICT"; break;
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
          , m_wq(new WorkQueue<WorkItem>(num_threads))
      {
        if (m_pool_name.length() > 15)
          m_pool_name.resize(15);
      }

      virtual ~WorkerPool() {
        stop_thread_pool();
        delete m_wq;
      }

      void send_work(const WorkItem& work) {
        m_wq->enqueue(work);
      }

      WorkItem get_work() {
        return m_wq->dequeue();
      }

      bool wq_is_empty( void ) {
        return m_wq->is_empty();
      }

      void start_thread_pool() {
        UMAP_LOG(Debug, "Starting " <<  m_pool_name << " Pool of "
            << m_num_threads << " threads");

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
        UMAP_LOG(Debug, "Stopping " <<  m_pool_name << " Pool of "
            << m_num_threads << " threads");

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

        UMAP_LOG(Debug, m_pool_name << " stopped");
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
  };
} // end of namespace Umap
#endif // _UMAP_WorkerPool_HPP
