//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_FlushManager_HPP
#define _UMAP_FlushManager_HPP

#include "umap/config.h"

#include <vector>

#include "umap/Buffer.hpp"
#include "umap/FlushWorkers.hpp"
#include "umap/Uffd.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/util/Macros.hpp"
#include "umap/store/Store.hpp"

namespace Umap {
  class FlushManager : public WorkerPool {
    public:
      FlushManager(
            uint64_t num_flushers, Buffer* buffer, Uffd* uffd, Store* store) :
              WorkerPool("Flush Manager", 1), m_buffer(buffer), m_store(store)
      {
        m_flush_workers = new FlushWorkers(PageRegion::getInstance()->get_num_flushers(), m_buffer, uffd);

        start_thread_pool();
      }

      ~FlushManager( void ) {
        FlushAll();
        stop_thread_pool();
        delete m_flush_workers;
      }

    private:
      Buffer* m_buffer;
      Store* m_store;
      FlushWorkers* m_flush_workers;

      void ThreadEntry() {
        FlushMgr();
      }

      void FlushMgr() {
        while ( 1 ) {
          auto w = get_work();

          if ( w.type == Umap::WorkItem::WorkType::EXIT )
            break;    // Time to leave

          m_buffer->lock();

          while ( ! m_buffer->flush_low_threshold_reached() ) {
            auto pd = m_buffer->get_oldest_present_page_descriptor(); // Could block

            if ( pd == nullptr )
              break;

            UMAP_LOG(Debug, m_buffer << ", " << pd);

            pd->set_state_leaving();

            WorkItem work;
            work.type = Umap::WorkItem::WorkType::FLUSH;
            work.page_desc = pd;

            if (pd->page_is_dirty())
              work.store = m_store;
            else
              work.store = nullptr;

            m_flush_workers->send_work(work);
          }

          m_buffer->unlock();
        }
      }

      void FlushAll( void ) {
        m_buffer->lock();
        for ( auto pd = m_buffer->get_oldest_present_page_descriptor(); pd != nullptr; pd = m_buffer->get_oldest_present_page_descriptor()) {
          pd->set_state_leaving();

          WorkItem work;

          work.type = Umap::WorkItem::WorkType::FLUSH;
          work.page_desc = pd;

          if (pd->page_is_dirty())
            work.store = m_store;
          else
            work.store = nullptr;

          m_flush_workers->send_work(work);
        }
        m_buffer->unlock();
      }
  };
} // end of namespace Umap

#endif // _UMAP_FlushManager_HPP
