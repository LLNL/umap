//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_FILE_HPP_
#define _UMAP_FILE_HPP_

#include <iostream>
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <fcntl.h>
#include "umap/umap.h"

namespace utility {

void* map_in_file(
    std::string filename,
    bool initonly,
    bool noinit,
    bool usemmap,
    uint64_t numbytes)
{
  int o_opts = O_RDWR | O_LARGEFILE | O_DIRECT;
  void* region = NULL;
  int fd;

  if ( initonly || !noinit ) {
    o_opts |= O_CREAT;
    unlink(filename.c_str());   // Remove the file if it exists
  }

  if ( ( fd = open(filename.c_str(), o_opts, S_IRUSR | S_IWUSR) ) == -1 ) {
    std::string estr = "Failed to open/create " + filename + ": ";
    perror(estr.c_str());
    return NULL;
  }

  if ( o_opts & O_CREAT ) {
    // If we are initializing, attempt to pre-allocate disk space for the file.
    try {
      int x;
      if ( ( x = posix_fallocate(fd, 0, numbytes) != 0 ) ) {
        std::ostringstream ss;
        ss << "Failed to pre-allocate " <<
          numbytes << " bytes in " << filename << ": ";
        perror(ss.str().c_str());
        return NULL;
      }
    } catch(const std::exception& e) {
      std::cerr << "posix_fallocate: " << e.what() << std::endl;
      return NULL;
    } catch(...) {
      std::cerr << "posix_fallocate failed to allocate backing store\n";
      return NULL;
    }
  }

  struct stat sbuf;
  if (fstat(fd, &sbuf) == -1) {
    std::string estr = "Failed to get status (fstat) for " + filename + ": ";
    perror(estr.c_str());
    return NULL;
  }

  if ( (off_t)sbuf.st_size != (numbytes) ) {
    std::cerr << filename << " size " << sbuf.st_size
      << " does not match specified data size of " << (numbytes) << std::endl;
    return NULL;
  }

  const int prot = PROT_READ|PROT_WRITE;

  if ( usemmap ) {
    region = mmap(NULL, numbytes, prot, MAP_SHARED | MAP_NORESERVE, fd, 0);
    if (region == MAP_FAILED) {
      std::ostringstream ss;
      ss << "mmap of " << numbytes << " bytes failed for " << filename << ": ";
      perror(ss.str().c_str());
      return NULL;
    }
  }
  else {
    int flags = UMAP_PRIVATE;

    region = umap(NULL, numbytes, prot, flags, fd, 0);
    if ( region == UMAP_FAILED ) {
        std::ostringstream ss;
        ss << "umap_mf of " << numbytes
          << " bytes failed for " << filename << ": ";
        perror(ss.str().c_str());
        return NULL;
    }
  }

  return region;
}

void unmap_file(bool usemmap, uint64_t numbytes, void* region)
{
  if ( usemmap ) {
    if ( munmap(region, numbytes) < 0 ) {
      std::ostringstream ss;
      ss << "munmap failure: ";
      perror(ss.str().c_str());
      exit(-1);
    }
  }
  else {
    if (uunmap(region, numbytes) < 0) {
      std::ostringstream ss;
      ss << "uunmap of failure: ";
      perror(ss.str().c_str());
      exit(-1);
    }
  }
}

}
#endif
