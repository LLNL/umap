///////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_Flushers_HPP
#define _UMAP_Flushers_HPP

#include "umap/config.h"

#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "umap/Buffer.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/Uffd.hpp"
#include "umap/util/Macros.hpp"
#include "umap/store/Store.hpp"

namespace Umap {

struct FlushWorkItem {
  void* region;
  PageDescriptor* page_desk;
  Store* store;   // Set to nullptr if no I/O required
};

class Flushers : public WorkerPool {
  public:
    Flushers(uint64_t num_flushers, Buffer* buffer, Uffd* uffd)
      :   WorkerPool("UMAP Flushers", num_flushers), m_buffer(buffer)
        , m_uffd(uffd)
    {
      start_thread_pool();
    }

    ~Flushers( void ) {
    }

  private:
    Buffer* m_buffer;
    Uffd* m_uffd;

    inline void ThreadEntry() {
      FlushersLoop();
    }

    void FlushersLoop( void ) {
      uint64_t page_size = PageRegion::getInstance()->get_umap_page_size();

      while ( ! time_to_stop_thread_pool() ) {
        try {
          auto w = get_work();
          auto page_addr = w.page_desc->get_page_addr();

          if ( w.store != nullptr ) {
            uint64_t offset = m_uffd->get_offset(page_addr);
            m_uffd->enable_write_protect(page_addr);

            UMAP_LOG(Debug, "Flushing page: " << page_addr);
            if (w.store->write_to_store((char*)page_addr, page_size, offset) == -1)
              UMAP_ERROR("write_to_store failed: " << errno << " (" << strerror(errno) << ")");
          }

          if (madvise(page_addr, page_size, MADV_DONTNEED) == -1)
            UMAP_ERROR("madvise failed: " << errno << " (" << strerror(errno) << ")");

          m_buffer->lock();
          m_buffer->mark_page_not_present(w.page_desc);
          m_buffer->unlock();
        }
        catch (...) {
          continue;
        }
      }
    }
};
} // end of namespace Umap

#endif // _UMAP_Flushers_HPP
