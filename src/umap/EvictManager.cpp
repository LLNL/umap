//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
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

void EvictManager::EvictMgr( void ) {
  while ( 1 ) {
    auto w = get_work();

    if ( w.type == Umap::WorkItem::WorkType::EXIT )
      break;    // Time to leave

    while ( ! m_buffer->low_threshold_reached() ) {
#if 0
      WorkItem work;
      work.type = Umap::WorkItem::WorkType::EVICT;
      work.page_desc = m_buffer->evict_oldest_page(); // Could block

      if ( work.page_desc == nullptr )
        break;

      UMAP_LOG(Debug, m_buffer << ", " << work.page_desc);

      m_evict_workers->send_work(work);
#else
      std::vector<PageDescriptor*> evicted_pages = m_buffer->evict_oldest_pages();
      for(auto pd : evicted_pages){
        WorkItem work;
        work.type = Umap::WorkItem::WorkType::EVICT;
        work.page_desc = pd;
        assert( work.page_desc != nullptr );
        m_evict_workers->send_work(work);
      }
#endif
    }
  }
}
void EvictManager::WaitAll( void )
{
  UMAP_LOG(Debug, "Entered");
  m_evict_workers->wait_for_idle();
  UMAP_LOG(Debug, "Done");
}
  
void EvictManager::EvictAll( void )
{
  UMAP_LOG(Debug, "Entered");

  for (auto pd = m_buffer->evict_oldest_page(); pd != nullptr; pd = m_buffer->evict_oldest_page()) {
    UMAP_LOG(Debug, "evicting: " << pd);
    if (pd->dirty) {
      WorkItem work = { .page_desc = pd, .type = Umap::WorkItem::WorkType::FAST_EVICT };
      m_evict_workers->send_work(work);
    }
    else {
      m_buffer->mark_page_as_free(pd);
    }
  }

  m_evict_workers->wait_for_idle();

  UMAP_LOG(Debug, "Done");
}

void EvictManager::schedule_eviction(PageDescriptor* pd)
{
  WorkItem work = { .page_desc = pd, .type = Umap::WorkItem::WorkType::EVICT };

  m_evict_workers->send_work(work);
}

void EvictManager::schedule_flush(PageDescriptor* pd)
{
  WorkItem work = { .page_desc = pd, .type = Umap::WorkItem::WorkType::FLUSH };

  m_evict_workers->send_work(work);
}

EvictManager::EvictManager( void ) :
        WorkerPool("Evict Manager", 1)
      , m_buffer(RegionManager::getInstance().get_buffer_h())
{
  m_evict_workers = new EvictWorkers(  RegionManager::getInstance().get_num_evictors()
                                     , m_buffer, RegionManager::getInstance().get_uffd_h());
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
