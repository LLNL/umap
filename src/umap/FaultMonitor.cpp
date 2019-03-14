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
#include <pthread.h>
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

namespace Umap {
  FaultMonitor::FaultMonitor(
            Store*   store
          , char*    region
          , uint64_t region_size
          , char*    mmap_region
          , uint64_t mmap_region_size
          , uint64_t page_size
          , uint64_t max_fault_events
        ) :   m_store(store)
            , m_region(region)
            , m_region_size(region_size)
            , m_mmap_region(mmap_region)
            , m_mmap_region_size(mmap_region_size)
            , m_page_size(page_size)
            , m_max_fault_events(max_fault_events)
            , m_time_to_stop(false)
  {
    m_uffd = new Uffd(region, region_size, max_fault_events, page_size);

    start_thread();
  }

  FaultMonitor::~FaultMonitor( void )
  {
    stop_thread();
    delete m_uffd;
  }

  void FaultMonitor::start_thread()
  {
    int error = pthread_create(&m_monitor, NULL, monitor_thread_starter, this);

    if (error)
      UMAP_ERROR("pthread_create failed: " << strerror(error));
  }

  void FaultMonitor::stop_thread()
  {
    UMAP_LOG(Debug, "Stoping " << (void*)m_monitor);
    m_time_to_stop = true;
    (void) pthread_join(m_monitor, NULL);
  }

  void FaultMonitor::monitor_thread() {
    UMAP_LOG(Debug, "\nThe UFFD Monitor says hello: "
        << "\n             m_store: " <<  (void*)m_store
        << "\n            m_region: " <<  (void*)m_region
        << "\n       m_region_size: " <<  m_region_size
        << "\n       m_mmap_region: " <<  (void*)m_mmap_region
        << "\n  m_mmap_region_size: " <<  m_mmap_region_size
        << "\n         m_page_size: " <<  m_page_size
        << "\n  m_max_fault_events: " <<  m_max_fault_events
        << "\n           m_uffd_fd: " <<  m_uffd_fd
        << "\n           m_monitor: " <<  m_monitor
    );

    while ( ! m_time_to_stop ) {
      if (m_uffd->get_page_events() == false)
        continue;
    }

    UMAP_LOG(Debug, "Bye");
  }

  void* FaultMonitor::monitor_thread_starter(void * This) {
    ((FaultMonitor *)This)->monitor_thread();
    return NULL;
  }
} // end of namespace Umap
