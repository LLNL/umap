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
#include "umap/WorkQueue.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {
class PageFiller : WorkerPool {
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
              , m_region(region)
              , m_region_size(region_size)
              , m_mmap_region(mmap_region)
              , m_mmap_region_size(mmap_region_size)
              , m_page_size(page_size)
              , m_max_fault_events(max_fault_events)
    {
      m_uffd = new Uffd(region, region_size, max_fault_events, page_size);
      m_page_fill_wq = new WorkQueue<FillWorkItem>;

      m_buffer = new Buffer(
            PageRegion::getInstance()->get_max_pages_in_buffer()
          , PageRegion::getInstance()->get_flush_threshold()
      );

      m_page_fillers = new Fillers(m_uffd , m_store, m_page_fill_wq);

      m_page_flusher = new PageFlusher(
            PageRegion::getInstance()->get_num_flushers()
          , m_buffer, m_uffd, m_store
      );

      start_thread_pool();
    }

    ~PageFiller( void )
    {
      delete m_page_flusher;
      delete m_page_fillers;
      delete m_buffer;
      delete m_page_fill_wq;
      delete m_uffd;
    }

  protected:
    inline void ThreadEntry() {
      UMAP_LOG(Debug, "\nPageFiller says Hello: "
          << "\n             m_store: " <<  (void*)m_store
          << "\n            m_region: " <<  (void*)m_region
          << "\n       m_region_size: " <<  m_region_size
          << "\n       m_mmap_region: " <<  (void*)m_mmap_region
          << "\n  m_mmap_region_size: " <<  m_mmap_region_size
          << "\n         m_page_size: " <<  m_page_size
          << "\n  m_max_fault_events: " <<  m_max_fault_events
          << "\n           m_uffd_fd: " <<  m_uffd_fd
      );

      while ( ! time_to_stop_thread_pool() ) {
        auto pe = m_uffd->get_page_events();

        UMAP_LOG(Debug, "Recieved " << pe.size() << " page events");
        if (pe.size() == 0)
          continue;

        //
        // Make sure we have enough available slots in the buffer
        //
      }
    }

  private:
    Store*    m_store;
    char*     m_region;
    uint64_t  m_region_size;
    char*     m_mmap_region;
    uint64_t  m_mmap_region_size;
    uint64_t  m_page_size;
    uint64_t  m_max_fault_events;
    int       m_uffd_fd;

    Uffd* m_uffd;

    WorkQueue<FillWorkItem>*  m_page_fill_wq;
    Fillers* m_page_fillers;

    PageFlusher* m_page_flusher;
    Buffer* m_buffer;
};
} // end of namespace Umap

#endif // _UMAP_PageFiller_HPP
