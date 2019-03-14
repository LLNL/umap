//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include <unistd.h>
#include <stdio.h>
#include "umap/store/Store.hpp"
#include "StoreFile.h"
#include <iostream>
#include <sstream>
#include <string>

#ifdef UMAP_DEBUG_LOGGING
#include <string.h>
#endif

namespace Umap {
  StoreFile::StoreFile(void* _region_, size_t _rsize_, size_t _alignsize_, int _fd_)
    : region{_region_}, rsize{_rsize_}, alignsize{_alignsize_}, fd{_fd_}
  {
  }

  ssize_t StoreFile::read_from_store(char* buf, size_t nb, off_t off)
  {
    size_t rval = 0;
#ifdef UMAP_DEBUG_LOGGING
    std::stringstream ss;
    ss << "pread(fd=" << fd
      << ", buf=" << (void*)buf
      << ", nb=" << nb
      << ", off=" << off
      << ")";
    debug_printf("%s\n", ss.str().c_str());
#endif
    rval = pread(fd, buf, nb, off);
#ifdef UMAP_DEBUG_LOGGING
    if (rval == -1) {
      int eno = errno;
      std::cerr << ss.str() << ": " << strerror(eno) << std::endl;
      _exit(1);
    }
#endif
    return rval;
  }

  ssize_t  StoreFile::write_to_store(char* buf, size_t nb, off_t off)
  {
    size_t rval = 0;
#ifdef UMAP_DEBUG_LOGGING
    std::stringstream ss;
    ss << "pwrite(fd=" << fd
      << ", buf=" << (void*)buf
      << ", nb=" << nb
      << ", off=" << off
      << ")";
    debug_printf("%s\n", ss.str().c_str());
#endif
    rval = pwrite(fd, buf, nb, off);
#ifdef UMAP_DEBUG_LOGGING
    if (rval == -1) {
      int eno = errno;
      std::cerr << ss.str() << ": " << strerror(eno) << std::endl;
      _exit(1);
    }
#endif
    return rval;
  }
}
