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
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <omp.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include "errno.h"
#include "umap/umap.h"
#include "caliper/cali_datatracker.h"

using namespace std;

int
open_prealloc_file( const char* fname, uint64_t totalbytes)
{

  int fd = open(fname, O_RDWR | O_LARGEFILE | O_DIRECT | O_CREAT, S_IRUSR | S_IWUSR);
  if ( fd == -1 ) {
    int eno = errno;
    std::cerr << "Failed to create " << fname << ": " << strerror(eno) << std::endl;
    exit(1);
  }

  // Pre-allocate disk space for the file.
  try {
    int x;
    if ( ( x = posix_fallocate(fd, 0, totalbytes) != 0 ) ) {
      int eno = errno;
      std::cerr << "Failed to pre-allocate " << fname << ": " << strerror(eno) << std::endl;
      exit(1);
    }
  } catch(const std::exception& e) {
    std::cerr << "posix_fallocate: " << e.what() << std::endl;
    exit(1);
  } catch(...) {
    int eno = errno;
    std::cerr << "Failed to pre-allocate " << fname << ": " << strerror(eno) << std::endl;
    exit(1);
  }

  void* temp = malloc(totalbytes);
  memset(temp, '1', totalbytes);
  ssize_t res = write(fd, temp, totalbytes); 
  assert(res == totalbytes);
  close(fd);

  fd = open(fname, O_RDONLY, S_IRUSR | S_IWUSR);
  if ( fd == -1 ) {
    int eno = errno;
    std::cerr << "Failed to create " << fname << ": " << strerror(eno) << std::endl;
    exit(1);
  }  
  
  return fd;
}


int
main(int argc, char **argv)
{
  const char* filename = argv[1];

  uint64_t umap_pagesize = umapcfg_get_umap_page_size();
  const uint64_t umap_region_length = 4 * umap_pagesize;
  int fd = open_prealloc_file(filename, umap_region_length);

  std::cout << "umap_pagesize "  << umap_pagesize << "\n";
  std::cout << "umap_region_length "  << umap_region_length << "\n";
  std::cout << "open_prealloc_file "<< filename << "\n";

  void* base_addr = umap(NULL, umap_region_length, PROT_READ, UMAP_PRIVATE, fd, 0);
  if ( base_addr == UMAP_FAILED ) {
    int eno = errno;
    std::cerr << "Failed to umap " << filename << ": " << strerror(eno) << std::endl;
    return -1;
  }
  std::cout << "umap base_addr at "<< base_addr <<"\n";
  cali_datatracker_track(base_addr, "umap", umap_region_length);


  /* Update to the in-core buffer*/
  char *arr = (char *) base_addr;
  size_t array_size = umap_region_length/umap_pagesize;
#pragma omp parallel for
  for(size_t i=0; i < umap_region_length; i=i+umap_pagesize){
    char c = arr[i];
    std::cout << "Array["<<i<<"] = " << c <<"\n";
  }
  
  if (uunmap(base_addr, umap_region_length) < 0) {
    int eno = errno;
    std::cerr << "Failed to uumap " << filename << ": " << strerror(eno) << std::endl;
    return -1;
  }

  close(fd);

  cali_datatracker_untrack(base_addr);
  
  std::cout << "file closed "<< filename << "\n";;
  
  return 0;
}
