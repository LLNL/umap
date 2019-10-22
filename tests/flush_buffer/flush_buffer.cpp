//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

/*
 * It is a simple example showing flush_cache ensures that 
 * modified pages in buffer is persisted into back stores.
 */
#include <iostream>
#include <parallel/algorithm>
#include <fcntl.h>
#include <omp.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include "errno.h"
#include "umap/umap.h"

#define FLUSH_BUF 1

int
open_prealloc_file( const char* fname, uint64_t totalbytes)
{
  if ( unlink(fname) ) {
    int eno = errno;
    if ( eno != ENOENT ) {
      std::cerr << "Failed to unlink " << fname << ": " 
		<< strerror(eno) << " Errno=" << eno << std::endl;
    }
    return -1;
  }

  int fd = open(fname, O_RDWR | O_LARGEFILE | O_DIRECT | O_CREAT, S_IRUSR | S_IWUSR);
  if ( fd == -1 ) {
    int eno = errno;
    std::cerr << "Failed to create " << fname << ": " << strerror(eno) << std::endl;
    return -1;
  }

  // Pre-allocate disk space for the file.
  try {
    int x;
    if ( ( x = posix_fallocate(fd, 0, totalbytes) != 0 ) ) {
      int eno = errno;
      std::cerr << "Failed to pre-allocate " << fname << ": " << strerror(eno) << std::endl;
      return -1;
    }
  } catch(const std::exception& e) {
    std::cerr << "posix_fallocate: " << e.what() << std::endl;
    return -1;
  } catch(...) {
    int eno = errno;
    std::cerr << "Failed to pre-allocate " << fname << ": " << strerror(eno) << std::endl;
    return -1;
  }

  return fd;
}


int
main(int argc, char **argv)
{
  const char* filename = argv[1];

  uint64_t umap_pagesize = umapcfg_get_umap_page_size();
  std::cout << "umap_pagesize "  << umap_pagesize << "\n";

  const uint64_t umap_region_length = 1024 * 1024 * 4;
  std::cout << "umap_region_length "  << umap_region_length << "\n";

  assert(umap_region_length % umap_pagesize == 0);

  int fd = open_prealloc_file(filename, umap_region_length);
  std::cout << "open_prealloc_file "<< filename << "\n";

  void* base_addr = umap(NULL, umap_region_length, PROT_READ|PROT_WRITE, UMAP_PRIVATE, fd, 0);
  if ( base_addr == UMAP_FAILED ) {
    int eno = errno;
    std::cerr << "Failed to umap " << filename << ": " << strerror(eno) << std::endl;
    return -1;
  }
  std::cout << "umap base_addr at "<< base_addr <<"\n";


  /* Update to the in-core buffer*/
  const char tmp[] = "Hello"; 
  strcpy((char*)base_addr, tmp);

#ifdef  FLUSH_BUF  
  if (umap_msync(base_addr, 4096, MS_SYNC) < 0) {
    std::cerr << "Failed to flush cache to " << filename << std::endl;
    return -1;
  }
  std::cout << "umap_msync \n";
#endif

  close(fd);
  std::cout << "file closed before uunmap \n";

  /*
  if (uunmap(base_addr, umap_region_length) < 0) {
    int eno = errno;
    std::cerr << "Failed to uumap " << filename << ": " << strerror(eno) << std::endl;
    return -1;
  }
  std::cout << "uunmap base_addr at "<< base_addr <<"\n";

  close(fd);
  std::cout << "file closed "<< filename << "\n";;
  */
  return 0;
}
