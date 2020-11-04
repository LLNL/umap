//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include <unistd.h>
#include <stdio.h>
#include "StoreFile.h"
#include <iostream>
#include <sstream>
#include <string.h>

#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {
  StoreFile::StoreFile(void* _region_, size_t _rsize_, size_t _alignsize_, int _fd_)
    : region{_region_}, rsize{_rsize_}, alignsize{_alignsize_}, fd{_fd_}
  {
    UMAP_LOG(Debug,
        "region: " << region << " rsize: " << rsize
        << " alignsize: " << alignsize << " fd: " << fd);
  }

  ssize_t StoreFile::read_from_store(char* buf, size_t nb, off_t off)
  {
    size_t rval = 0;

    UMAP_LOG(Debug, "pread(fd=" << fd << ", buf=" << (void*)buf
                    << ", nb=" << nb << ", off=" << off << ")";);

    rval = pread(fd, buf, nb, off);

    if (rval == -1) {
      int eno = errno;
      UMAP_ERROR("pread(fd=" << fd << ", buf=" << (void*)buf
                      << ", nb=" << nb << ", off=" << off
                      << "): Failed - " << strerror(eno));
    }
    return rval;
  }

  ssize_t  StoreFile::write_to_store(char* buf, size_t nb, off_t off)
  {
    size_t rval = 0;

    UMAP_LOG(Debug, "pwrite(fd=" << fd << ", buf=" << (void*)buf
                    << ", nb=" << nb << ", off=" << off << ")";);

    rval = pwrite(fd, buf, nb, off);
    if (rval == -1) {
      int eno = errno;
      UMAP_ERROR("pwrite(fd=" << fd << ", buf=" << (void*)buf
                      << ", nb=" << nb << ", off=" << off
                      << "): Failed - " << strerror(eno));
    }
    return rval;
  }
}
