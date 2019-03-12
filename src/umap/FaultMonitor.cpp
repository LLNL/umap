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
#include <linux/userfaultfd.h>  // ioctl(UFFDIO_*)
#include <string.h>             // strerror()
#include <sys/ioctl.h>          // ioctl()
#include <sys/syscall.h>        // syscall()
#include <unistd.h>             // syscall()
#include <fcntl.h>

#ifndef UFFDIO_COPY_MODE_WP
#define UMAP_RO_MODE
#endif

#include "umap/config.h"

#include "umap/FaultMonitor.hpp"
#include "umap/util/Macros.hpp"
#include "umap/util/Pthread.hpp"
#include "umap/store/Store.hpp"

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
{
  if ((m_uffd_fd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK)) < 0) {
    UMAP_ERROR("userfaultfd syscall not available in this kernel");
  }

  check_uffd_compatibility();
  register_uffd();
}

FaultMonitor::~FaultMonitor( void )
{
}

void FaultMonitor::check_uffd_compatibility( void )
{
  struct uffdio_api uffdio_api = {
      .api = UFFD_API
#ifdef UMAP_RO_MODE
    , .features = 0
#else
    , .features = UFFD_FEATURE_PAGEFAULT_FLAG_WP
#endif

    , .ioctls = 0
  };

  if (ioctl(m_uffd_fd, UFFDIO_API, &uffdio_api) == -1) {
    UMAP_ERROR("ioctl(UFFDIO_API) Failed: " << strerror(errno));
  }

#ifndef UMAP_RO_MODE
  if ( !(uffdio_api.features & UFFD_FEATURE_PAGEFAULT_FLAG_WP) ) {
    UMAP_ERROR("UFFD Compatibilty Check - unsupported userfaultfd WP");
  }
#endif
}

void FaultMonitor::register_uffd( void )
{
  struct uffdio_register uffdio_register = {
    .range = {  .start = (uint64_t)m_region
              , .len = m_region_size}
#ifndef UMAP_RO_MODE
              , .mode = UFFDIO_REGISTER_MODE_MISSING | UFFDIO_REGISTER_MODE_WP
#else
              , .mode = UFFDIO_REGISTER_MODE_MISSING
#endif
  };

  UMAP_LOG(Debug,
    "Register " << (uffdio_register.range.len / m_page_size)
    << " Pages from: " << (void*)(uffdio_register.range.start)
    << " - " << (void*)(uffdio_register.range.start + (uffdio_register.range.len-1)));

  if (ioctl(m_uffd_fd, UFFDIO_REGISTER, &uffdio_register) == -1) {
    UMAP_ERROR("ioctl(UFFDIO_REGISTER) failed: " << strerror(errno));
  }

  if ((uffdio_register.ioctls & UFFD_API_RANGE_IOCTLS) != UFFD_API_RANGE_IOCTLS) {
    UMAP_ERROR("unexpected userfaultfd ioctl set: " << uffdio_register.ioctls);
  }
}
} // end of namespace Umap
