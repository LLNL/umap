//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

#include "umap/Buffer.hpp"
#include "umap/BufferManager.hpp"
#include "umap/EvictManager.hpp"
#include "umap/EvictWorkers.hpp"
#include "umap/RegionManager.hpp"
#include "umap/Uffd.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/util/Macros.hpp"
#include "umap/store/Store.hpp"

namespace Umap {

void EvictManager::EvictMgr( void ) {
  while ( 1 ) {
    auto w = get_work();

    if ( w.type == Umap::WorkItem::WorkType::EXIT )
      break;    // Time to leave

    while ( ! w.buffer->low_threshold_reached() ) {
      WorkItem work;
      work.type = Umap::WorkItem::WorkType::EVICT;
      work.page_desc = w.buffer->evict_oldest_page(); // Could block
      work.buffer = w.buffer;

      if ( work.page_desc == nullptr )
        break;

      UMAP_LOG(Debug, w.buffer << ", " << work.page_desc);

      m_evict_workers->send_work(work);
    }
  }
}

void EvictManager::EvictAll( void )
{
  UMAP_LOG(Debug, "Entered");
  RegionManager::getInstance()->get_buffer_manager_h()->evictAll(m_evict_workers);
  UMAP_LOG(Debug, "Done");
}

void EvictManager::schedule_eviction(PageDescriptor* pd, Buffer* bd)
{
  WorkItem work = { .page_desc = pd, .buffer = bd, .type = Umap::WorkItem::WorkType::EVICT };

  m_evict_workers->send_work(work);
}

EvictManager::EvictManager( void ) : WorkerPool("Evict Manager", 1)
{
  m_evict_workers = new EvictWorkers(  RegionManager::getInstance()->get_num_evictors()
                                     , RegionManager::getInstance()->get_uffd_h());
  start_thread_pool();
}

EvictManager::~EvictManager( void ) {
  UMAP_LOG(Debug, "Calling EvictAll");
  EvictAll();
  UMAP_LOG(Debug, "Calling stop_thread_pool");
  stop_thread_pool();
  UMAP_LOG(Debug, "Deleting eviction workers");
  delete m_evict_workers;
  UMAP_LOG(Debug, "Done");
}

void EvictManager::ThreadEntry() {
  EvictMgr();
}

} // end of namespace Umap
