//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_Uffd_HPP
#define _UMAP_Uffd_HPP

#include <algorithm>            // sort()
#include <cassert>              // assert()
#include <cstdint>              // uint64_t
#include <vector>               // We all have lists to manage

#include <errno.h>              // strerror()
#include <fcntl.h>              // O_CLOEXEC
#include <linux/userfaultfd.h>  // ioctl(UFFDIO_*)
#include <poll.h>               // poll()
#include <string.h>             // strerror()
#include <sys/ioctl.h>          // ioctl()
#include <sys/syscall.h>        // syscall()
#include <unistd.h>             // syscall()

#ifndef UFFDIO_COPY_MODE_WP
#define UMAP_RO_MODE
#endif

#include "umap/config.h"
#include "umap/Uffd.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {

struct PageEvent {
  void* aligned_page_address;
  bool is_write_fault;
};

struct less_than_key {
  inline bool operator() (const struct uffd_msg& lhs, const struct uffd_msg& rhs)
  {
    if (lhs.arg.pagefault.address == rhs.arg.pagefault.address)
      return (lhs.arg.pagefault.flags >= rhs.arg.pagefault.flags);
    else
      return (lhs.arg.pagefault.address < rhs.arg.pagefault.address);
  }
};

class Uffd {
  public:
    Uffd(   char*    region
          , uint64_t region_size
          , uint64_t max_fault_events
          , uint64_t page_size
      ) :   m_region(region)
          , m_region_size(region_size)
          , m_max_fault_events(max_fault_events)
          , m_page_size(page_size)
    {
      UMAP_LOG(Debug, "Uffd:"
          << "\n               region: " 
          << (void*)m_region << " - " << (void*)(m_region+(m_region_size-1))
          << "\n          region size: " << m_region_size
          << "\n maximum fault events: " << m_max_fault_events
          << "\n            page size: " << m_page_size
      );

      if ((m_uffd_fd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK)) < 0)
        UMAP_ERROR("userfaultfd syscall not available in this kernel: "
            << strerror(errno));

      check_uffd_compatibility();
      register_with_uffd();
      m_events.resize(m_max_fault_events);
    }

    ~Uffd( void ) {
      unregister_from_uffd();
    }

    std::vector<PageEvent> get_page_events( void ) {
      std::vector<PageEvent> rval;
      struct pollfd pollfd = { .fd = m_uffd_fd, .events = POLLIN };

      int pollres = poll(&pollfd, 1, 2000);

      switch (pollres) {
        case 0:
          return rval;
        case 1:
          break;
        case -1:
          UMAP_ERROR("poll failed: " << strerror(errno));
        default:
          UMAP_ERROR("poll: unexpected result: " << pollres);
      }

      if (pollfd.revents & POLLERR)
        UMAP_ERROR("POLLERR: ");

      if ( !(pollfd.revents & POLLIN) )
        return rval;

      int readres = read(m_uffd_fd, &m_events[0], m_max_fault_events * sizeof(struct uffd_msg));

      if (readres == -1) {
        if (errno == EAGAIN)
          return rval;

        UMAP_ERROR("read failed: " << strerror(errno));
      }

      assert("Invalid read result returned" && 
                (readres % sizeof(struct uffd_msg) == 0));

      int msgs = readres / sizeof(struct uffd_msg);

      assert("invalid message size" && msgs >= 1);

      //
      // Since uffd page events arrive on the system page boundary which could
      // be different from umap's page size, the page address for the incoming
      // events are adjusted to the beginning of the umap page address.  The
      // events are then sorted in page base address / operation type order and
      // are processed only once while duplicates are skipped.
      //
      for (int i = 0; i < msgs; ++i)
        m_events[i].arg.pagefault.address &= ~(m_page_size-1);

      std::sort(m_events.begin(), m_events.begin()+msgs, less_than_key());

      uint64_t last_addr = 0;
      for (int i = 0; i < msgs; ++i) {
        if (m_events[i].arg.pagefault.address == last_addr)
          continue;

        last_addr = m_events[i].arg.pagefault.address;

        PageEvent pe;
        pe.aligned_page_address = (char*)(m_events[i].arg.pagefault.address);
#ifndef UMAP_RO_MODE
        pe.is_write_fault = (m_events[i].arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE) != 0);
#else
        pe.is_write_fault = false;
#endif
        rval.push_back(pe);
      }
      return rval;
    }

    void  enable_write_protect(
              void*
#ifndef UMAP_RO_MODE
              page_address
#endif
          )
    {
#ifndef UMAP_RO_MODE
      struct uffdio_writeprotect wp = {
          .range = { .start = (uint64_t)page_address, .len = m_page_size }
        , .mode = UFFDIO_WRITEPROTECT_MODE_WP
      };

      if (ioctl(m_uffd_fd, UFFDIO_WRITEPROTECT, &wp) == -1)
        UMAP_ERROR("ioctl(UFFDIO_WRITEPROTECT): " << strerror(errno));
#endif // UMAP_RO_MODE
    }

    void disable_write_protect(
      void*
#ifndef UMAP_RO_MODE
      page_address
#endif
    )
    {
#ifndef UMAP_RO_MODE
      struct uffdio_writeprotect wp = {
          .range = { .start = (uint64_t)page_address, .len = m_page_size }
        , .mode = 0
      };

      if (ioctl(m_uffd_fd, UFFDIO_WRITEPROTECT, &wp) == -1)
        UMAP_ERROR("ioctl(UFFDIO_WRITEPROTECT): " << strerror(errno));
#endif // UMAP_RO_MODE
    }

    void copy_in_page(char* data, void* page_address) {
      UMAP_LOG(Debug, "(page_address = " << page_address << ")");
      struct uffdio_copy copy = {
          .dst = (uint64_t)page_address
        , .src = (uint64_t)data
        , .len = m_page_size
        , .mode = 0
      };

      if (ioctl(m_uffd_fd, UFFDIO_COPY, &copy) == -1)
        UMAP_ERROR("UFFDIO_COPY failed: " << strerror(errno));
    }

    void copy_in_page_and_write_protect(char* data, void* page_address) {
      UMAP_LOG(Debug, "(page_address = " << page_address << ")");
      struct uffdio_copy copy = {
          .dst = (uint64_t)page_address
        , .src = (uint64_t)data
        , .len = m_page_size
#ifndef UMAP_RO_MODE
        , .mode = UFFDIO_COPY_MODE_WP
#else
        , .mode = 0
#endif
      };

      if (ioctl(m_uffd_fd, UFFDIO_COPY, &copy) == -1)
        UMAP_ERROR("UFFDIO_COPY failed: " << strerror(errno));
    }

    uint64_t get_offset(void* page) {
      return ( (uint64_t)page - (uint64_t)m_region );
    }

    
  private:
    char*    m_region;
    uint64_t m_region_size;
    uint64_t m_max_fault_events;
    uint64_t m_page_size;
    int      m_uffd_fd;

    std::vector<uffd_msg> m_events;

    void check_uffd_compatibility( void ) {
      struct uffdio_api uffdio_api = {
          .api = UFFD_API
#ifdef UMAP_RO_MODE
        , .features = 0
#else
        , .features = UFFD_FEATURE_PAGEFAULT_FLAG_WP
#endif

        , .ioctls = 0
      };

    if (ioctl(m_uffd_fd, UFFDIO_API, &uffdio_api) == -1)
      UMAP_ERROR("ioctl(UFFDIO_API) Failed: " << strerror(errno));

#ifndef UMAP_RO_MODE
    if ( !(uffdio_api.features & UFFD_FEATURE_PAGEFAULT_FLAG_WP) )
      UMAP_ERROR("UFFD Compatibilty Check - unsupported userfaultfd WP");
#endif
    }

    void register_with_uffd( void ) {
      struct uffdio_register uffdio_register = {
          .range = {  .start = (uint64_t)m_region, .len = m_region_size }
#ifndef UMAP_RO_MODE
        , .mode = UFFDIO_REGISTER_MODE_MISSING | UFFDIO_REGISTER_MODE_WP
#else
        , .mode = UFFDIO_REGISTER_MODE_MISSING
#endif
      };

      UMAP_LOG(Debug,
        "Registering " << (uffdio_register.range.len / m_page_size)
        << " pages from: " << (void*)(uffdio_register.range.start)
        << " - " << (void*)(uffdio_register.range.start +
                                  (uffdio_register.range.len-1)));

      if (ioctl(m_uffd_fd, UFFDIO_REGISTER, &uffdio_register) == -1)
        UMAP_ERROR("ioctl(UFFDIO_REGISTER) failed: " << strerror(errno));

      if ((uffdio_register.ioctls & UFFD_API_RANGE_IOCTLS) != UFFD_API_RANGE_IOCTLS)
        UMAP_ERROR("unexpected userfaultfd ioctl set: " << uffdio_register.ioctls);
    }

    void unregister_from_uffd(void) {
      struct uffdio_register uffdio_register = {
          .range = { .start = (uint64_t)m_region, .len = m_region_size }
        , .mode = 0
      };

      UMAP_LOG(Debug,
        "Unregistering " << (uffdio_register.range.len / m_page_size)
        << " pages from: " << (void*)(uffdio_register.range.start)
        << " - " << (void*)(uffdio_register.range.start +
                                  (uffdio_register.range.len-1)));

      if (ioctl(m_uffd_fd, UFFDIO_UNREGISTER, &uffdio_register.range))
        UMAP_ERROR("ioctl(UFFDIO_UNREGISTER) failed: " << strerror(errno));
    }
};
} // end of namespace Umap

#endif // _UMAP_Uffd_HPP
