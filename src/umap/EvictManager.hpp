//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_EvictManager_HPP
#define _UMAP_EvictManager_HPP

#include "umap/config.h"

#include <vector>

#include "umap/Buffer.hpp"
#include "umap/EvictWorkers.hpp"
#include "umap/Region.hpp"
#include "umap/Uffd.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/util/Macros.hpp"
#include "umap/store/Store.hpp"

namespace Umap {
  class EvictManager : public WorkerPool {
    private:
      Buffer* m_buffer;
      Store* m_store;
      EvictWorkers* m_evict_workers;

      void EvictMgr() {
        while ( 1 ) {
          auto w = get_work();

          if ( w.type == Umap::WorkItem::WorkType::EXIT )
            break;    // Time to leave

          while ( ! m_buffer->evict_low_threshold_reached() ) {
            WorkItem work;
            work.type = Umap::WorkItem::WorkType::EVICT;
            work.page_desc = m_buffer->evict_oldest_page(); // Could block

            if ( work.page_desc == nullptr )
              break;

            UMAP_LOG(Debug, m_buffer << ", " << work.page_desc);

            if (work.page_desc->page_is_dirty())
              work.store = m_store;
            else
              work.store = nullptr;

            m_evict_workers->send_work(work);
          }
        }
      }

      void EvictAll( void ) {
        for ( auto pd = m_buffer->evict_oldest_page();
              pd != nullptr;
              pd = m_buffer->evict_oldest_page()) {
          WorkItem work;

          work.type = Umap::WorkItem::WorkType::EVICT;
          work.page_desc = pd;

          if (pd->page_is_dirty())
            work.store = m_store;
          else
            work.store = nullptr;

          m_evict_workers->send_work(work);
        }
      }

      void ThreadEntry() {
        EvictMgr();
      }
    public:
      EvictManager(
            uint64_t num_evictors, Buffer* buffer, Uffd* uffd, Store* store) :
              WorkerPool("Evict Manager", 1), m_buffer(buffer), m_store(store)
      {
        m_evict_workers = new EvictWorkers(  Region::getInstance()->get_num_evictors()
                                           , m_buffer, uffd);
        start_thread_pool();
      }

      ~EvictManager( void ) {
        EvictAll();
        stop_thread_pool();
        delete m_evict_workers;
      }
  };
} // end of namespace Umap
#endif // _UMAP_EvictManager_HPP
