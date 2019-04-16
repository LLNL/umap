//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_Fillers_HPP
#define _UMAP_Fillers_HPP

#include "umap/config.h"

#include <cstdint>              // calloc
#include <errno.h>
#include <string.h>             // strerror()
#include <unistd.h>

#include "umap/WorkerPool.hpp"
#include "umap/Uffd.hpp"
#include "umap/WorkQueue.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {
  struct FillWorkItem {
    PageDescriptor* page_desc;
  };

  class Fillers : WorkerPool {
    public:
      Fillers(Uffd* uffd, Store* store, WorkQueue<FillWorkItem>* wq)
        :   WorkerPool("Page Fillers", PageRegion::getInstance()->get_num_fillers())
          , m_uffd(uffd), m_store(store), m_wq(wq)
      {
        start_thread_pool();
      }

      ~Fillers( void ) {
        UMAP_LOG(Debug, "Stopping the Fillers");
        m_wq->kill();
        UMAP_LOG(Debug, "Workers should have been stopped now");
      }

    private:
      Uffd* m_uffd;
      Store* m_store;
      WorkQueue<FillWorkItem>* m_wq;

      inline void ThreadEntry() {
        char* copyin_buf;
        uint64_t page_size = PageRegion::getInstance()->get_umap_page_size();

        if (posix_memalign((void**)&copyin_buf, page_size, page_size*2)) {
          UMAP_ERROR("posix_memalign failed to allocated "
              << page_size*2 << " bytes of memory");
        }

        if (copyin_buf == nullptr) {
          UMAP_ERROR("posix_memalign failed to allocated "
              << page_size*2 << " bytes of memory");
        }

        while ( ! time_to_stop_thread_pool() ) {
          try {
            auto w = m_wq->dequeue();

            uint64_t offset = m_uffd->get_offset(w.page_desc->page);

            UMAP_LOG(Debug, "Filling page: " << (void*)w.page_desc->page);
            if (m_store->read_from_store(copyin_buf, page_size, offset) == -1)
              UMAP_ERROR("read_from_store failed");

            if ( ! w.page_desc->is_dirty ) {
              UMAP_LOG(Debug, "Copyin (WP) page: " << (void*)w.page_desc->page);
              m_uffd->copy_in_page_and_write_protect(copyin_buf, w.page_desc->page);
            }
            else {
              UMAP_LOG(Debug, "Copyin page: " << (void*)w.page_desc->page);
              m_uffd->copy_in_page(copyin_buf, w.page_desc->page);
            }

            // TODO: Should we lock around this state change?
            w.page_desc->state = PageDescriptor::State::PRESENT;

            // TODO: Do we need to nofity anyone that this is now done?
          }
          catch ( ... ) {
            ;
          }
        }

        free(copyin_buf);
      }
  };
} // end of namespace Umap
#endif // _UMAP_Fillers_HPP
