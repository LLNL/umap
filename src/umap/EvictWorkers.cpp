///////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "umap/Buffer.hpp"
#include "umap/EvictWorkers.hpp"
#include "umap/RegionManager.hpp"
#include "umap/Uffd.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/util/Macros.hpp"

static int tid_g = 0;

namespace Umap {
void EvictWorkers::EvictWorker( void )
{
  int t_id = tid_g;
  tid_g ++;

  while ( 1 ) {
    auto w = get_work(t_id);
    if ( w.type == Umap::WorkItem::WorkType::EXIT )
      break;    // Time to leave

    auto pd = w.page_desc;
    RegionDescriptor *rd = pd->region;
    uint64_t psize = rd->page_size();
    char    *paddr = pd->page;
//#ifdef PROF
    //UMAP_LOG(Info, " "<<t_id<<" : " << w << " psize " << psize << " " << m_buffer);
//#endif

    // write back dirty pages to data store
    if ( pd->dirty ) {
      auto store  = rd->store();
      auto offset = rd->store_offset(paddr);

      m_uffd->enable_write_protect(psize, paddr);
      
      if (store && store->write_to_store(paddr, psize, offset) == -1)
        UMAP_ERROR("write_to_store failed: "
            << errno << " (" << strerror(errno) << ")");

      pd->dirty = false;
    }

    // flush without evicting from the buffer
    if (w.type == Umap::WorkItem::WorkType::FLUSH)
      continue;
    
    // evict from the buffer
    if (madvise(paddr, psize, MADV_DONTNEED) == -1)
      UMAP_ERROR("madvise failed: " << errno << " (" << strerror(errno) << ")");

    m_buffer->mark_page_as_free(pd);

  }
}

EvictWorkers::EvictWorkers(uint64_t num_evictors, Buffer* buffer, Uffd* uffd)
  :   WorkerPool("Evict Workers", num_evictors), m_buffer(buffer)
    , m_uffd(uffd)
{
  start_thread_pool();
}

EvictWorkers::~EvictWorkers( void )
{
  stop_thread_pool();
}

void EvictWorkers::ThreadEntry( void )
{
  EvictWorkers::EvictWorker();
}
} // end of namespace Umap
