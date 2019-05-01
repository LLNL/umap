//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_FillWorkers_HPP
#define _UMAP_FillWorkers_HPP

#include "umap/config.h"

#include <cstdint>              // calloc
#include <errno.h>
#include <string.h>             // strerror()
#include <unistd.h>

#include "umap/Buffer.hpp"
#include "umap/Region.hpp"
#include "umap/Uffd.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {
  class FillWorkers : public WorkerPool {
    private:
      void FillWorker( void ) {
        char* copyin_buf;
        uint64_t page_size = Region::getInstance()->get_umap_page_size();
        uint64_t read_ahead = Region::getInstance()->get_read_ahead();
        std::size_t sz = 2 * page_size;

        if (posix_memalign((void**)&copyin_buf, page_size, sz)) {
          UMAP_ERROR("posix_memalign failed to allocated "
              << sz << " bytes of memory");
        }

        if (copyin_buf == nullptr) {
          UMAP_ERROR("posix_memalign failed to allocated "
              << sz << " bytes of memory");
        }

        while ( 1 ) {
          auto w = get_work();

          UMAP_LOG(Debug, ": " << w << " " << m_buffer);

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

          m_buffer->make_page_present(w.page_desc);
        }

        free(copyin_buf);
      }

      void ThreadEntry( void ) {
        FillWorker();
      }

      Uffd* m_uffd;
      Buffer* m_buffer;
      uint64_t m_read_ahead;

    public:
      FillWorkers(Uffd* uffd, Buffer* buffer)
        :   WorkerPool("Fill Workers", Region::getInstance()->get_num_fillers())
          , m_uffd(uffd)
          , m_buffer(buffer)
          , m_read_ahead(Region::getInstance()->get_read_ahead())
      {
        start_thread_pool();
      }

      ~FillWorkers( void ) {
        stop_thread_pool();
      }
  };
} // end of namespace Umap
#endif // _UMAP_FillWorker_HPP
