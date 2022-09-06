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
      std::vector<PageDescriptor*> evicted_pages = m_buffer->evict_oldest_pages();
      for(auto pd : evicted_pages){
        WorkItem work;
        work.type = Umap::WorkItem::WorkType::EVICT;
        work.page_desc = pd;
        m_evict_workers->send_work(work);
      }
    }
  }
}
void EvictManager::WaitAll( void )
{
  m_evict_workers->wait_for_idle();
}
  
void EvictManager::EvictAll( void )
{
  //while ( ! m_buffer->low_threshold_reached() ){}
  
  std::vector<PageDescriptor*> evicted_pages = m_buffer->evict_oldest_pages();

  while( evicted_pages.size() > 0 ){
    for(auto pd : evicted_pages){
      WorkItem work;
      work.type = Umap::WorkItem::WorkType::EVICT;
      work.page_desc = pd;
      m_evict_workers->send_work(work);
    }
    evicted_pages = m_buffer->evict_oldest_pages();
  }

  m_evict_workers->wait_for_idle();

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

EvictManager::EvictManager( RegionManager& rm ) :
        WorkerPool("Evict Manager", 1)
      , m_buffer(rm.get_buffer_h())
      , m_rm( rm )
{
  m_evict_workers = new EvictWorkers(  m_rm.get_num_evictors()
                                     , m_buffer, m_rm.get_uffd_h());
  start_thread_pool();
}

EvictManager::~EvictManager( void ) {
  EvictAll();
  stop_thread_pool();
  delete m_evict_workers;
}

void EvictManager::ThreadEntry() {
  EvictMgr();
}

} // end of namespace Umap
