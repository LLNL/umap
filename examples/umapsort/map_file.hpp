/* This file is part of UMAP.  For copyright information see the COPYRIGHT
 * file in the top level directory, or at
 * https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free
 * software; you can redistribute it and/or modify it under the terms of the
 * GNU Lesser General Public License (as published by the Free Software
 * Foundation) version 2.1 dated February 1999.  This program is distributed in
 * the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the terms and conditions of the GNU Lesser General Public License for
 * more details.  You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _MAP_FILE_HPP_
#define _MAP_FILE_HPP_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <iostream>
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <fcntl.h>
#include "umap/umap.h"

namespace utility {

void* map_file(
    std::string filename,
    bool need_init,  
    bool has_write,
    bool usemmap,
    uint64_t numbytes,
    void* start_addr=nullptr)
{
  int o_opts = O_LARGEFILE | O_DIRECT;
  void* region = NULL;
  int fd;

  if ( need_init ) {
    o_opts |= O_CREAT;
    has_write = true;
    unlink(filename.c_str());   // Remove the file if it exists
  }

  if(has_write)
    o_opts |= O_RDWR;
  else
    o_opts |= O_RDONLY;

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
        ss << "Failed to pre-allocate " << numbytes << " bytes in " << filename << ": ";
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

  const int prot = (has_write) ?(PROT_READ|PROT_WRITE) : PROT_READ;

  if ( usemmap ) {
    int flags = MAP_SHARED | MAP_NORESERVE;

    if (start_addr != nullptr)
      flags |= MAP_FIXED;

    region = mmap(start_addr, numbytes, prot, flags, fd, 0);
    if (region == MAP_FAILED) {
      std::ostringstream ss;
      ss << "mmap of " << numbytes << " bytes failed for " << filename << ": ";
      perror(ss.str().c_str());
      return NULL;
    }
  }
  else {
    int flags = UMAP_PRIVATE;

    if (start_addr != nullptr)
      flags |= MAP_FIXED;

    region = umap(start_addr, numbytes, prot, flags, fd, 0);
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
