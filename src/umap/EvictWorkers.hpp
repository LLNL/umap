///////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_EvictWorkers_HPP
#define _UMAP_EvictWorkers_HPP

#include "umap/config.h"

#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "umap/Buffer.hpp"
#include "umap/Region.hpp"
#include "umap/Uffd.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/util/Macros.hpp"
#include "umap/store/Store.hpp"

namespace Umap {

class EvictWorkers : public WorkerPool {
  private:
    Buffer* m_buffer;
    Uffd* m_uffd;

    void EvictWorker( void ) {
      uint64_t page_size = Region::getInstance()->get_umap_page_size();

      while ( 1 ) {
        auto w = get_work();

        UMAP_LOG(Debug, " " << w << " " << m_buffer);

        if ( w.type == Umap::WorkItem::WorkType::EXIT )
          break;    // Time to leave

        auto page_addr = w.page_desc->get_page_addr();

        if ( w.store != nullptr ) {
          uint64_t offset = m_uffd->get_offset(page_addr);
          m_uffd->enable_write_protect(page_addr);

          if (w.store->write_to_store((char*)page_addr, page_size, offset) == -1)
            UMAP_ERROR("write_to_store failed: " << errno << " (" << strerror(errno) << ")");
        }

        if (madvise(page_addr, page_size, MADV_DONTNEED) == -1)
          UMAP_ERROR("madvise failed: " << errno << " (" << strerror(errno) << ")");

        m_buffer->remove_page(w.page_desc);
      }
    }

    inline void ThreadEntry() {
      EvictWorker();
    }

  public:
    EvictWorkers(uint64_t num_evictors, Buffer* buffer, Uffd* uffd)
      :   WorkerPool("Evict Workers", num_evictors), m_buffer(buffer)
        , m_uffd(uffd)
    {
      start_thread_pool();
    }

    ~EvictWorkers( void ) {
      stop_thread_pool();
    }
};
} // end of namespace Umap
#endif // _UMAP_EvictWorkers_HPP
