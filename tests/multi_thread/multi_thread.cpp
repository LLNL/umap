//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
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
#include <sstream>
#include "errno.h"
#include "umap/umap.h"

#define FLUSH_BUF 1

using namespace std;

int
open_prealloc_file( const char* fname, uint64_t totalbytes)
{
  /*
  int status = unlink(fname);
  int eno = errno;
  if ( status!=0 && eno != ENOENT ) {
    std::cerr << "Failed to unlink " << fname << ": "<< strerror(eno) << " Errno=" << eno <<std::endl;
    exit(1);
  }
  */

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

  return fd;
}


int
main(int argc, char **argv)
{
  const char* filename_prefix = argv[1];

  uint64_t umap_pagesize = umapcfg_get_umap_page_size();
  std::cout << "umap_pagesize "  << umap_pagesize << "\n";

  const uint64_t umap_region_length = 1 * umap_pagesize;
  std::cout << "umap_region_length "  << umap_region_length << "\n";

  assert(umap_region_length % umap_pagesize == 0);

#pragma omp parallel 
  {
    int tid = omp_get_thread_num();
    std::stringstream ss;
    ss << filename_prefix << "_t" << tid;
    std::string str = ss.str();
    const char* filename = str.c_str();
    int fd = open_prealloc_file(filename, umap_region_length);
    std::cout << "open_prealloc_file "<< filename << "\n";

    void* base_addr = umap(NULL, umap_region_length, PROT_READ|PROT_WRITE, UMAP_PRIVATE, fd, 0);
    if ( base_addr == UMAP_FAILED ) {
      int eno = errno;
      std::cerr << "Thread "<< tid << " Failed to umap " << filename << ": " << strerror(eno) << std::endl;
      exit(1);
    }
    std::cout << "Thread "<< tid << " umap base_addr at "<< base_addr <<"\n";


    /* Update to the in-core buffer*/
    uint64_t *arr = (uint64_t *) base_addr;
    size_t array_size = umap_region_length/sizeof(uint64_t);
    for(size_t i=array_size/2; i < array_size; i++)
      arr[i] = (uint64_t)(i + tid*10000);
    std::cout << "Thread "<< tid << " Update Array of "<< array_size <<" uint64_t\n";
  

#ifdef  FLUSH_BUF  
  if (umap_flush() < 0) {
    std::cerr << "Failed to flush cache to " << filename << std::endl;
    exit(1);
  }
  std::cout << "umap_flush done\n";
#endif

  //close(fd);
  std::cout << "open "<< filename <<" separately to read content before calling uunmap \n";
  ifstream rf(filename, ios::in | ios::binary);
  if(!rf) {
    std::cout << "Cannot open file!" << std::endl;
    exit(1);
  }
  uint64_t arr_in[array_size];
  rf.read((char *) &arr_in[0], sizeof(uint64_t)*array_size);
  rf.close();
  for(int i=0; i < array_size; i++) {
    std::cout << "Arr["<< i <<"]: " <<arr_in[i] << std::endl;
  }


  std::cout << "Access from UMap region \n";
  size_t stride = umap_pagesize/sizeof(uint64_t);
  for(size_t i=0; i < array_size; i=i+stride)
    std::cout << "Read array["<< i <<"] = "<< arr[i] << std::endl;


  std::cout << "Call uunmap \n";
  if (uunmap(base_addr, umap_region_length) < 0) {
    int eno = errno;
    std::cerr << "Failed to uumap " << filename << ": " << strerror(eno) << std::endl;
    exit(1);
  }

  close(fd);
  std::cout << "file closed "<< filename << "\n";;
  }
  return 0;
}
