//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_PageFlusher_HPP
#define _UMAP_PageFlusher_HPP

#include "umap/config.h"

#include "umap/Buffer.hpp"
#include "umap/FlushWorkers.hpp"
#include "umap/Uffd.hpp"
#include "umap/util/Macros.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/PthreadPool.hpp"
#include "umap/util/WorkQueue.hpp"

namespace Umap {
  class PageFlusher : PthreadPool {
    public:
      PageFlusher(
            uint64_t num_workers, Buffer* buffer, Uffd* uffd, Store* store) :
              PthreadPool("Page Flusher", 1), m_buffer(buffer), m_uffd(uffd), m_store(store)
      {
        m_flush_wq = new WorkQueue<FlushWorkItem>;
        m_page_flush_workers = new FlushWorkers(PageRegion::getInstance()->get_num_flush_workers(), m_buffer, m_uffd , m_store, m_flush_wq);

        start_thread_pool();
      }

      ~PageFlusher( void )
      {
        delete m_flush_wq;
        delete m_page_flush_workers;
        
      }

    private:
      Buffer* m_buffer;
      Uffd* m_uffd;
      Store* m_store;
      WorkQueue<FlushWorkItem>* m_flush_wq;
      FlushWorkers* m_page_flush_workers;

      inline void ThreadEntry() {
        while ( ! time_to_stop_thread_pool() ) {
          sleep(1);
        }
      }
  };
} // end of namespace Umap

#endif // _UMAP_PageFlusher_HPP
