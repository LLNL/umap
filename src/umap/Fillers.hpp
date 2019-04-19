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

#include "umap/Buffer.hpp"
#include "umap/Uffd.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {
  class Fillers : public WorkerPool {
    public:
      Fillers(Uffd* uffd, Buffer* buffer)
        :   WorkerPool("Page Fillers", PageRegion::getInstance()->get_num_fillers())
          , m_uffd(uffd)
          , m_buffer(buffer)
      {
        start_thread_pool();
      }

      ~Fillers( void ) {
        stop_thread_pool();
      }

    private:
      Uffd* m_uffd;
      Buffer* m_buffer;

      void ThreadEntry( void ) {
        FillWorker();
      }

      void FillWorker( void ) {
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

        while ( 1 ) {
          auto w = get_work();

          UMAP_LOG(Debug, " " << w << " " << m_buffer);

          if (w.type == Umap::WorkItem::WorkType::EXIT)
            break;    // Time to leave

          if ( w.store == nullptr ) {
            //
            // The only reason we would not be given a store object is
            // when a present page has become dirty.  At this point, the only
            // thing we do is disable write protect on the present page and
            // wake up the faulting thread
            //
            m_uffd->disable_write_protect(w.page_desc->get_page_addr());
          }
          else {
            uint64_t offset = m_uffd->get_offset(w.page_desc->get_page_addr());

            if (w.store->read_from_store(copyin_buf, page_size, offset) == -1)
              UMAP_ERROR("read_from_store failed");

            if ( ! w.page_desc->page_is_dirty() ) {
              m_uffd->copy_in_page_and_write_protect(copyin_buf, w.page_desc->get_page_addr());
            }
            else {
              m_uffd->copy_in_page(copyin_buf, w.page_desc->get_page_addr());
            }
          }
        }

        free(copyin_buf);
      }
  };
} // end of namespace Umap
#endif // _UMAP_Fillers_HPP
