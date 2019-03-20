//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <cstdint>
#include <errno.h>
#include <fcntl.h>
#include <string.h>             // strerror()
#include <unistd.h>             // syscall()
#include <linux/userfaultfd.h>  // ioctl(UFFDIO_*)
#include <sys/ioctl.h>          // ioctl()
#include <sys/syscall.h>        // syscall()

#ifndef UFFDIO_COPY_MODE_WP
#define UMAP_RO_MODE
#endif

#include "umap/config.h"

#include "umap/FaultMonitor.hpp"
#include "umap/Uffd.hpp"

#include "umap/store/Store.hpp"

#include "umap/util/Macros.hpp"
#include "umap/util/PthreadPool.hpp"

namespace Umap {
  FaultMonitor::FaultMonitor(
            Store*   store
          , char*    region
          , uint64_t region_size
          , char*    mmap_region
          , uint64_t mmap_region_size
          , uint64_t page_size
          , uint64_t max_fault_events
        ) :   PthreadPool(1)
            , m_store(store)
            , m_region(region)
            , m_region_size(region_size)
            , m_mmap_region(mmap_region)
            , m_mmap_region_size(mmap_region_size)
            , m_page_size(page_size)
            , m_max_fault_events(max_fault_events)
  {
    m_uffd = new Uffd(region, region_size, max_fault_events, page_size);
    m_pagein_wq = new WorkQueue<PageInWorkItem>;
    m_pageout_wq = new WorkQueue<PageOutWorkItem>;

    start_thread_pool();
  }

  FaultMonitor::~FaultMonitor( void )
  {
    delete m_pageout_wq;
    delete m_pagein_wq;
    delete m_uffd;
  }

  void FaultMonitor::ThreadEntry() {
    UMAP_LOG(Debug, "\nThe UFFD Monitor says hello: "
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
      if (m_uffd->get_page_events() == false)
        continue;
    }

    UMAP_LOG(Debug, "Bye");
  }
} // end of namespace Umap
