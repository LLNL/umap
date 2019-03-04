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
#include "umap/store/Store.hpp"

namespace Umap {

FaultMonitor::FaultMonitor(
        char* _region_base_address
      , uint64_t _region_size_in_bytes
      , bool _read_only
      , uint64_t _page_size
      , uint64_t _max_uffd_events
      ) :   m_region_base_address(_region_base_address)
          , m_region_size_in_bytes(_region_size_in_bytes)
          , m_read_only(_read_only)
          , m_page_size(_page_size)
          , m_max_uffd_events(_max_uffd_events)
            // TODO: Add reference to PageInWorkQueue
            // TODO: Add reference to FreePageDescriptor list
{
#ifdef UMAP_RO_MODE
  if ( ! m_read_only ) {
    UMAP_ERROR("Write operations not allowed in Read-Only build");
  }
#endif // UMAP_RO_MODE

  // Confirm we are running on a kernel that supports userfaultfd
  //
  if ((m_uffd_fd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK)) < 0) {
    UMAP_ERROR("userfaultfd syscall not available in this kernel");
  }

  // Confirm that the userfaultfd interface supports callbacks we need
  //
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

  // Register ourselves with userfaultfd on the given vm range
  //
  struct uffdio_register uffdio_register = {
    .range = {  .start = (uint64_t)m_region_base_address
              , .len = m_region_size_in_bytes}
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
