///////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
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

namespace Umap {
void EvictWorkers::EvictWorker( void )
{
  uint64_t page_size = RegionManager::getInstance().get_umap_page_size();

  while ( 1 ) {
    auto w = get_work();

    UMAP_LOG(Debug, " " << w << " " << m_buffer);

    if ( w.type == Umap::WorkItem::WorkType::EXIT )
      break;    // Time to leave

    auto pd = w.page_desc;

    if ( m_uffd && pd->dirty ) {
      auto store = pd->region->store();
      auto offset = pd->region->store_offset(pd->page);

      m_uffd->enable_write_protect(pd->page);

      if (store->write_to_store(pd->page, page_size, offset) == -1)
        UMAP_ERROR("write_to_store failed: "
            << errno << " (" << strerror(errno) << ")");

      pd->dirty = false;
    }

    if (w.type == Umap::WorkItem::WorkType::FLUSH)
      continue;
    
    if (w.type != Umap::WorkItem::WorkType::FAST_EVICT) {
      if (madvise(pd->page, page_size, MADV_REMOVE) == -1)
        UMAP_ERROR("madvise failed: " << errno << " (" << strerror(errno) << ")");
    }

    UMAP_LOG(Debug, "Removing page: " << w.page_desc);
    m_buffer->mark_page_as_free(w.page_desc);
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
