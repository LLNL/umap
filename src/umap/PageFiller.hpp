//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_PageFiller_HPP
#define _UMAP_PageFiller_HPP

#include "umap/config.h"

#include <cstdint>
#include <errno.h>
#include <fcntl.h>
#include <string.h>             // strerror()
#include <unistd.h>             // syscall()
#include <vector>
#include <linux/userfaultfd.h>  // ioctl(UFFDIO_*)
#include <sys/ioctl.h>          // ioctl()
#include <sys/syscall.h>        // syscall()

#include "umap/config.h"

#include "umap/Buffer.hpp"
#include "umap/PageRegion.hpp"
#include "umap/Fillers.hpp"
#include "umap/PageFlusher.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/Uffd.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {
  class PageFiller : public WorkerPool {
    public:
      PageFiller(
                Store*   store
              , char*    region
              , uint64_t region_size
              , char*    mmap_region
              , uint64_t mmap_region_size
              , uint64_t page_size
              , uint64_t max_fault_events
            ) :   WorkerPool("Page Filler", 1)
                , m_store(store)
                , m_page_size(page_size)
                , m_max_fault_events(max_fault_events)
      {
        m_uffd = new Uffd(region, region_size, max_fault_events, page_size);

        m_buffer = new Buffer(
              PageRegion::getInstance()->get_max_pages_in_buffer()
            , PageRegion::getInstance()->get_flush_low_water_threshold()
            , PageRegion::getInstance()->get_flush_high_water_threshold()
        );

        m_page_fillers = new Fillers(m_uffd);

        m_page_flusher = new PageFlusher(
              PageRegion::getInstance()->get_num_flushers()
            , m_buffer, m_uffd, m_store
        );

        start_thread_pool();
      }

      ~PageFiller( void )
      {
        m_uffd->stop_uffd();
        stop_thread_pool();
        delete m_page_fillers;
        delete m_page_flusher;
        delete m_buffer;
        delete m_uffd;
      }

    protected:
      void ThreadEntry() {
        PageFillerLoop();
      }

      void PageFillerLoop() {
        UMAP_LOG(Debug,    "\n             m_store: " <<  (void*)m_store
                        << "\n         m_page_size: " <<  m_page_size
                        << "\n  m_max_fault_events: " <<  m_max_fault_events
                        << "\n           m_uffd_fd: " <<  m_uffd_fd
        );

        while ( 1 ) {
          auto pe = m_uffd->get_page_events();

          if (pe.size() == 0)
            continue;

          if ( pe[0].aligned_page_address == (char*)nullptr && pe[0].is_write_fault == false) {
            UMAP_LOG(Debug, "Good-bye");
            break;
          }

          m_buffer->lock();

          for ( auto & event : pe ) {
            WorkItem work;
            auto pd = m_buffer->page_already_present(event.aligned_page_address);

            if ( pd != nullptr ) {  // Page is already present
              if (event.is_write_fault && pd->page_is_dirty() == false) {
                work.page_desc = pd; work.store = nullptr;
                pd->mark_page_dirty();
                UMAP_LOG(Debug, "Present: " << pd);
              }
              else {
                UMAP_LOG(Debug, "Spurious: " << pd);
                continue;           // Spurious
              }
            }
            else {                  // This page has not been brought in yet
              pd = m_buffer->get_page_descriptor(event.aligned_page_address);
              work.page_desc = pd; work.store = m_store;

              m_buffer->mark_page_present(pd);

              if (event.is_write_fault)
                pd->mark_page_dirty();

              UMAP_LOG(Debug, "New: " << pd << " From: " << m_buffer);
            }

            m_page_fillers->send_work(work);
          }

          m_buffer->unlock();
        }
      }

    private:
      Store*    m_store;
      uint64_t  m_page_size;
      uint64_t  m_max_fault_events;
      int       m_uffd_fd;

      Uffd* m_uffd;

      Fillers* m_page_fillers;

      PageFlusher* m_page_flusher;
      Buffer* m_buffer;
  };
} // end of namespace Umap

#endif // _UMAP_PageFiller_HPP
