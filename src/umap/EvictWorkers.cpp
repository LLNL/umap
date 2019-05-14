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
  EvictWorkers::EvictWorkers(uint64_t num_evictors, Buffer* buffer, Uffd* uffd)
    :   WorkerPool("Evict Workers", num_evictors), m_buffer(buffer)
      , m_uffd(uffd)
  {
    start_thread_pool();
  }

  EvictWorkers::~EvictWorkers( void ) {
    stop_thread_pool();
  }

  void EvictWorkers::EvictWorker( void ) {
    uint64_t page_size = RegionManager::getInstance()->get_umap_page_size();

    while ( 1 ) {
      auto w = get_work();

      UMAP_LOG(Debug, " " << w << " " << m_buffer);

      if ( w.type == Umap::WorkItem::WorkType::EXIT )
        break;    // Time to leave

      auto page_addr = w.page_desc->page;

      if ( w.store != nullptr ) {
        uint64_t offset = w.offset;
        m_uffd->enable_write_protect(page_addr);

        if (w.store->write_to_store((char*)page_addr, page_size, offset) == -1)
          UMAP_ERROR("write_to_store failed: " << errno << " (" << strerror(errno) << ")");
      }

      if (madvise(page_addr, page_size, MADV_DONTNEED) == -1)
        UMAP_ERROR("madvise failed: " << errno << " (" << strerror(errno) << ")");

      m_buffer->remove_page(w.page_desc);
    }
  }

  void EvictWorkers::ThreadEntry( void ) {
    EvictWorkers::EvictWorker();
  }
} // end of namespace Umap
