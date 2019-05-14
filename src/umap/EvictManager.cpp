//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

#include "umap/Buffer.hpp"
#include "umap/EvictManager.hpp"
#include "umap/EvictWorkers.hpp"
#include "umap/RegionManager.hpp"
#include "umap/Uffd.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/util/Macros.hpp"
#include "umap/store/Store.hpp"

namespace Umap {

EvictManager::EvictManager( void ) :
        WorkerPool("Evict Manager", 1)
      , m_buffer(RegionManager::getInstance()->get_buffer_h())
{
  m_evict_workers = new EvictWorkers(  RegionManager::getInstance()->get_num_evictors()
                                     , m_buffer, RegionManager::getInstance()->get_uffd_h());
  start_thread_pool();
}

EvictManager::~EvictManager( void ) {
  EvictAll();
  stop_thread_pool();
  delete m_evict_workers;
}

void EvictManager::EvictMgr( void ) {
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

      if (work.page_desc->is_dirty)
        work.store = work.page_desc->region->get_store();
      else
        work.store = nullptr;

      m_evict_workers->send_work(work);
    }
  }
}

void EvictManager::EvictAll( void ) {
  for ( auto pd = m_buffer->evict_oldest_page();
        pd != nullptr;
        pd = m_buffer->evict_oldest_page()) {
    WorkItem work;

    work.type = Umap::WorkItem::WorkType::EVICT;
    work.page_desc = pd;

    if (pd->is_dirty)
      work.store = pd->region->get_store();
    else
      work.store = nullptr;

    m_evict_workers->send_work(work);
  }
}

void EvictManager::ThreadEntry() {
  EvictMgr();
}

} // end of namespace Umap
