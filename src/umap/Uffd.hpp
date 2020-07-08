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
#include <iomanip>
#include <iostream>
#include <vector>               // We all have lists to manage
#include <map>

#include <errno.h>              // strerror()
#include <fcntl.h>              // O_CLOEXEC
#include <linux/userfaultfd.h>  // ioctl(UFFDIO_*)
#include <poll.h>               // poll()
#include <string.h>             // strerror()
#include <sys/ioctl.h>          // ioctl()
#include <sys/syscall.h>        // syscall()
#include <unistd.h>             // syscall()

#include "umap/config.h"
//
// The UFFDIO_COPY_MODE_WP is only defined in later versions of Linux (>5.0)
//
//#ifndef UFFDIO_COPY_MODE_WP
#define UMAP_RO_MODE
//#endif

#include "umap/RegionDescriptor.hpp"
#include "umap/RegionManager.hpp"
#include "umap/WorkerPool.hpp"

namespace Umap {
  class RegionManager;

  class PageEvent {
    public:
      PageEvent(void* paddr, bool iswrite);
  };

  class Uffd : public WorkerPool {
    public:
      Uffd( bool server = false, int client_uffd=-1);
      ~Uffd( void);

      void process_page(bool iswrite, char* addr );
      void register_region( RegionDescriptor* region, void *remote_addr=NULL);
      void unregister_region( RegionDescriptor* region, bool client_term=false);
      void release_buffer( RegionDescriptor* region );

      void  enable_write_protect( void* );
      void disable_write_protect( void* );
      void copy_in_page(char* data, void* page_address);
      void copy_in_page_and_write_protect(char* data, void* page_address);
      void wake_up_range( void* );

    private:
      RegionManager&        			m_rm;
      uint64_t              			m_max_fault_events;
      uint64_t              			m_page_size;
      Buffer*               			m_buffer;
      int                   			m_uffd_fd;
      int                   	    		m_pipe[2];
      std::vector<uffd_msg> 			m_events;
      std::map<void *, RegionDescriptor *>	m_rtol_map;
      bool                 			m_server;

      void uffd_handler( void );
      void ThreadEntry( void );
      void check_uffd_compatibility( void );
      void *get_remote_addr(void *);
      void *get_local_addr(void *);
  };
} // end of namespace Umap
#endif // _UMAP_Uffd_HPP
