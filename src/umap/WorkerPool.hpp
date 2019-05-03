//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_Pthread_HPP
#define _UMAP_Pthread_HPP

#ifndef _GNU_SOURCE
#define _GNU_SOURCE     // Needed for pthread_setname_np
#endif // GNU_SOURCE

#include <cstdint>
#include <pthread.h>
#include <string>
#include <vector>

#include "umap/PageDescriptor.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/WorkQueue.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {
  struct WorkItem {
    enum WorkType { NONE, EXIT, THRESHOLD, EVICT };
    PageDescriptor* page_desc;  // Set to nullptr if time to stop
    Store* store;               // Set to nullptr if no I/O required
    WorkType type;
  };

  std::ostream& operator<<(std::ostream& os, const Umap::WorkItem& b)
  {
    os << "{ page_desc: " << b.page_desc
       << ", store: " << b.store
       << ", type: " << b.type
       << " }";

    return os;
  }

  class WorkerPool {
    public:
      WorkerPool(const std::string& pool_name, uint64_t num_threads)
        :   m_pool_name(pool_name)
          , m_num_threads(num_threads)
          , m_wq(new WorkQueue<WorkItem>)
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

        WorkItem w = {.page_desc = nullptr, .store = nullptr, .type = Umap::WorkItem::WorkType::EXIT };

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