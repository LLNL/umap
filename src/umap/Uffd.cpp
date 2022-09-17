//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include <algorithm>            // sort()
#include <cassert>              // assert()
#include <cstdint>              // uint64_t
#include <iomanip>
#include <iostream>
#include <vector>               // We all have lists to manage

#include <errno.h>              // strerror()
#include <fcntl.h>              // O_CLOEXEC
#include <linux/userfaultfd.h>  // ioctl(UFFDIO_*)
#include <poll.h>               // poll()
#include <string.h>             // strerror()
#include <sys/ioctl.h>          // ioctl()
#include <sys/syscall.h>        // syscall()
#include <unistd.h>             // syscall()

#include "umap/config.h"
#include "umap/Uffd.hpp"
#include "umap/RegionDescriptor.hpp"
#include "umap/RegionManager.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {

struct less_than_key {
  inline bool operator() ( const uffd_msg& lhs, const uffd_msg& rhs ) {
    if (lhs.arg.pagefault.address == rhs.arg.pagefault.address)
      return (lhs.arg.pagefault.flags > rhs.arg.pagefault.flags);
    else
      return (lhs.arg.pagefault.address < rhs.arg.pagefault.address);
  }
};

void
Uffd::uffd_handler( void )
{
  struct pollfd pollfd[3] = {
      { .fd = m_uffd_fd, .events = POLLIN }
    , { .fd = m_pipe[0], .events = POLLIN }
    , { .fd = m_pipe[1], .events = POLLIN }
  };

  //
  // For the Uffd worker thread, we use our work queue as a sentinel for
  // when it is time to leave (since this particular thread gets its work
  // from the m_uffd_fd kernel module.
  //
  while ( wq_is_empty() ) {
    int pollres = poll(&pollfd[0], 3, -1);

    switch (pollres) {
      case 1:
        break;
      case -1:
        UMAP_ERROR("poll failed: " << strerror(errno));
      default:
        UMAP_ERROR("poll: unexpected result: " << pollres);
    }

    if (pollfd[1].revents & POLLIN || pollfd[2].revents & POLLIN)
      break;

    if (pollfd[0].revents & POLLERR)
      UMAP_ERROR("POLLERR: ");

    if ( !(pollfd[0].revents & POLLIN) )
      continue;

    int readres = read(m_uffd_fd, &m_events[0], m_max_fault_events * sizeof(struct uffd_msg));

    if (readres == -1) {
      if (errno == EAGAIN)
        continue;

      UMAP_ERROR("read failed: " << strerror(errno));
    }

    assert("Invalid read result returned" && (readres % sizeof(struct uffd_msg) == 0));

    int msgs = readres / sizeof(struct uffd_msg);

    assert("invalid message size" && msgs >= 1 && msgs <= m_max_fault_events);

    //
    // Since uffd page events arrive on the system page boundary which could
    // be different from umap's page size, the page address for the incoming
    // events are adjusted to the beginning of the umap page address.  The
    // events are then sorted in page base address / operation type order and
    // are processed only once while duplicates are skipped.
    //

    // support arbitrary umap page size (multiple of 4KB)
    std::sort(&m_events[0], &m_events[msgs], less_than_key());
#ifdef PROF
    for (int i = 0; i < msgs; ++i) 
      printf("\t %llx \n", m_events[i].arg.pagefault.address);
#endif

    auto     last_rd = m_rm.containing_region((char*)(m_events[0].arg.pagefault.address));
    uint64_t last_rd_page_size = last_rd->page_size();
    uint64_t last_rd_start = (uint64_t) last_rd->start();
    uint64_t last_rd_end   = (uint64_t) last_rd->end();
    uint64_t last_addr     = m_events[0].arg.pagefault.address;
#ifdef PROF
    printf("UFFD:: paddr %p (paddr - rd_start)=%d last_rd_page_size = %ld = %ld\n", 
          last_addr, (last_addr - last_rd_start), last_rd_page_size, ((last_addr - last_rd_start)%last_rd_page_size)); 
#endif
    last_addr = last_addr - (last_addr - last_rd_start) % last_rd_page_size;
    char*   addrs[msgs];
    bool iswrites[msgs];
    addrs[0] = (char*)(last_addr);
#ifndef UMAP_RO_MODE
    iswrites[0] = (m_events[0].arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE) != 0);
#else
    iswrites[0] = false;
#endif
    int p = 1;      

    for (int i = 1; i < msgs; ++i) {
      uint64_t addr = (m_events[i].arg.pagefault.address);

      if ( addr < (last_addr + last_rd_page_size) )
        continue;

      if( addr >= last_rd_end ){
        m_buffer->process_page_events(last_rd, addrs, iswrites, p);
        p=0;

        last_rd = m_rm.containing_region((char*)addr);
        last_rd_page_size = last_rd->page_size();
        last_rd_start = (uint64_t) last_rd->start();
        last_rd_end = (uint64_t) last_rd->end();
      }

      last_addr = addr - (addr - last_rd_start) % last_rd_page_size;
      
#ifndef UMAP_RO_MODE
      bool iswrite = (m_events[i].arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE) != 0);
#else
      bool iswrite = false;
#endif
      addrs[p] = (char*) last_addr;
      iswrites[p] = iswrite;
      p ++;
    }//end of msgs

    m_buffer->process_page_events(last_rd, addrs, iswrites, p);

  }//end of while

  UMAP_LOG(Debug, "Good bye");
}

/*void
Uffd::process_page( bool iswrite, char* addr )
{
  auto rd = m_rm.containing_region(addr);

  if ( rd != nullptr )
    m_buffer->process_page_event(addr, iswrite, rd);
}*/

void
Uffd::ThreadEntry()
{
  uffd_handler();
}

Uffd::Uffd(  RegionManager& rm  )
  :   WorkerPool("Uffd Manager", 1)
    , m_rm( rm ) //RegionManager::getInstance()
    , m_max_fault_events(m_rm.get_max_fault_events())
    , m_page_size(m_rm.get_umap_page_size())
    , m_buffer(m_rm.get_buffer_h())
{
  UMAP_LOG(Debug, "\n maximum fault events: " << m_max_fault_events
                  << "\n            page size: " << m_page_size);

  if ((m_uffd_fd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK)) < 0)
    UMAP_ERROR("userfaultfd syscall not available in this kernel: "
        << strerror(errno));

  if (pipe2(m_pipe, 0) < 0)
    UMAP_ERROR("userfaultfd pipe failed: " << strerror(errno));

  check_uffd_compatibility();
  m_events.resize(m_max_fault_events);

  start_thread_pool();
}

Uffd::~Uffd()
{
  char bye[5] = "bye";

  write(m_pipe[1], bye, 3);

  stop_thread_pool();
}

void
Uffd::enable_write_protect( uint64_t psize,
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

void
Uffd::disable_write_protect(uint64_t psize, 
  void*
#ifndef UMAP_RO_MODE
  page_address
#endif
)
{
#ifndef UMAP_RO_MODE
  struct uffdio_writeprotect wp = {
      .range = { .start = (uint64_t)page_address, .len = psize }
    , .mode = 0
  };

  if (ioctl(m_uffd_fd, UFFDIO_WRITEPROTECT, &wp) == -1)
    UMAP_ERROR("ioctl(UFFDIO_WRITEPROTECT): " << strerror(errno));
#endif // UMAP_RO_MODE
}

void
Uffd::copy_in_page(char* data, void* page_address, uint64_t psize)
{
  struct uffdio_copy copy = {
      .dst = (uint64_t)page_address
    , .src = (uint64_t)data
    , .len = psize
    , .mode = 0
  };

  if (ioctl(m_uffd_fd, UFFDIO_COPY, &copy) == -1)
    UMAP_ERROR("UFFDIO_COPY failed: " << strerror(errno));
}

void
Uffd::copy_in_page_and_write_protect(char* data, void* page_address, uint64_t psize)
{
  UMAP_LOG(Debug, "(page_address = " << page_address << ")");
  struct uffdio_copy copy = {
      .dst = (uint64_t)page_address
    , .src = (uint64_t)data
    , .len = psize
#ifndef UMAP_RO_MODE
    , .mode = UFFDIO_COPY_MODE_WP
#else
    , .mode = 0
#endif
  };

  if (ioctl(m_uffd_fd, UFFDIO_COPY, &copy) == -1) {
    UMAP_ERROR("UFFDIO_COPY failed @ " 
        << page_address << " : "
        << strerror(errno) << std::endl
    );
  }
}

void
Uffd::register_region( RegionDescriptor* rd )
{
  struct uffdio_register uffdio_register = {
      .range = {  .start = (__u64)(rd->start()), .len = rd->size() }
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

  if (ioctl(m_uffd_fd, UFFDIO_REGISTER, &uffdio_register) == -1) {
    UMAP_ERROR("ioctl(UFFDIO_REGISTER) failed: " << strerror(errno)
        << "Number of regions is: " << m_rm.get_num_active_regions()
    );
  }
  
  if( !(uffdio_register.ioctls & (1 << _UFFDIO_COPY))
#ifdef UFFDIO_WRITEPROTECT
      || !(uffdio_register.ioctls & (1 << _UFFDIO_WRITEPROTECT))
#endif
    )
    UMAP_ERROR("unexpected userfaultfd ioctl set: " << uffdio_register.ioctls);
}

void
Uffd::unregister_region( RegionDescriptor* rd )
{
  //
  // Make sure and evict any/all active pages from this region that are still
  // in the Buffer
  //
  m_buffer->evict_region(rd);

  struct uffdio_register uffdio_register = {
      .range = { .start = (__u64)(rd->start()), .len = rd->size() }
    , .mode = 0
  };

  UMAP_LOG(Debug,
    "Unregistering " << (uffdio_register.range.len / rd->page_size())
    << " pages (" << rd->page_size() << " bytes) from: " << (void*)(uffdio_register.range.start)
    << " - " << (void*)(uffdio_register.range.start + (uffdio_register.range.len-1)));

  if (ioctl(m_uffd_fd, UFFDIO_UNREGISTER, &uffdio_register.range))
    UMAP_ERROR("ioctl(UFFDIO_UNREGISTER) failed: " << strerror(errno));
}

void
Uffd::check_uffd_compatibility( void )
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

if (ioctl(m_uffd_fd, UFFDIO_API, &uffdio_api) == -1)
  UMAP_ERROR("ioctl(UFFDIO_API) Failed: " << strerror(errno));

#ifndef UMAP_RO_MODE
if ( !(uffdio_api.features & UFFD_FEATURE_PAGEFAULT_FLAG_WP) )
  UMAP_ERROR("UFFD Compatibilty Check - unsupported userfaultfd WP");
#endif
}
} // end of namespace Umap
