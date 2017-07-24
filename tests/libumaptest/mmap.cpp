/*
This file is part of UMAP.  For copyright information see the COPYRIGHT
file in the top level directory, or at
https://github.com/LLNL/umap/blob/master/COPYRIGHT
This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free
Software Foundation) version 2.1 dated February 1999.  This program is
distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the terms and conditions of the GNU Lesser General Public License
for more details.  You should have received a copy of the GNU Lesser General
Public License along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include "umap.h"
#include "umaptest.h"

using namespace std;

int umt_openandmap(const umt_optstruct_t* testops, uint64_t numbytes, void** region)
{
  int fd;
  int open_options = O_RDWR;

  if (testops->iodirect) 
    open_options |= O_DIRECT;

  if ( !testops->noinit )
    open_options |= O_CREAT;

#ifdef O_LARGEFILE
    open_options |= O_LARGEFILE;
#endif

  fd = open(testops->fn, open_options, S_IRUSR|S_IWUSR);
  if(fd == -1) {
    perror("open");
    exit(-1);
  }

  if (testops->noinit) {
    // If we are not initializing file, make sure that it is big enough
    struct stat sbuf;

    if (fstat(fd, &sbuf) == -1) {
      perror("fstat");
      exit(-1);
    }

    if ((uint64_t)sbuf.st_size < numbytes) {
      cerr << testops->fn 
        << " file is not large enough.  "  << sbuf.st_size 
        << " < size requested " << numbytes << endl;
      exit(-1);
    }
  }

  try {
      int x;
    if((x = posix_fallocate(fd,0, numbytes) != 0)) {
      perror("??posix_fallocate");

      cerr << "posix_fallocate(" << fd << ", 0, " << numbytes << ") returned " << x << endl;
      exit(-1);
    }
  } catch(const std::exception& e) {
      cerr << "posix_fallocate: " << e.what() << endl;
      exit(-1);
  } catch(...) {
      cerr << "posix_fallocate failed to instantiate _umap object\n";
      exit(-1);
  }

  int prot = PROT_READ|PROT_WRITE;

  if ( testops->usemmap ) {
    int flags = MAP_SHARED;

    *region = mmap(NULL, numbytes, prot, flags, fd, 0);
    if (*region == MAP_FAILED) {
      perror("mmap");
      exit(-1);
    }
  }
  else {
    int flags = UMAP_PRIVATE;

    *region = umap(NULL, numbytes, prot, flags, fd, 0);
    if (*region == UMAP_FAILED) {
      perror("umap");
      exit(-1);
    }
  }

  return fd;
}

void umt_closeandunmap(const umt_optstruct_t* testops, uint64_t numbytes, void* region, int fd)
{
  if ( testops->usemmap ) {
    if (munmap(region, numbytes) < 0) {
      perror("munmap");
      exit(-1);
    }
  }
  else {
    if (uunmap(region, numbytes) < 0) {
      perror("uunmap");
      exit(-1);
    }
  }

  close(fd);
}

