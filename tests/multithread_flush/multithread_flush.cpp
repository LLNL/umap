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

using namespace std;

int
open_prealloc_file( const char* fname, uint64_t totalbytes)
{
  
  int status = unlink(fname);
  int eno = errno;
  if ( status!=0 && eno != ENOENT ) {
    std::cerr << "Failed to unlink " << fname << ": "<< strerror(eno) << " Errno=" << eno <<std::endl;
    exit(1);
  }
  

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
  size_t num_pages;
  char* filename_prefix;

  if(argc==3){
    num_pages = atoi(argv[1]);
    filename_prefix = argv[2];
  }else{
    printf("%s filename_prefix num_pages_per_thread\n", argv[0]);
    return 0;
  }

  uint64_t umap_pagesize = umapcfg_get_umap_page_size();
  const uint64_t umap_region_length = num_pages * umap_pagesize;

#pragma omp parallel 
  {
    int tid = omp_get_thread_num();
    std::stringstream ss;
    ss << filename_prefix << "multithread_test_t" << tid;
    std::string str = ss.str();
    const char* filename = str.c_str();
    int fd = open_prealloc_file(filename, umap_region_length);
    std::cout << "tid "<<tid<<" open_prealloc_file "<< filename << " (" << umap_region_length <<" bytes)\n";


    void* base_addr = umap(NULL, umap_region_length, PROT_READ|PROT_WRITE, UMAP_PRIVATE, fd, 0);
    if ( base_addr == UMAP_FAILED ) {
      int eno = errno;
      std::cerr << "Thread "<< tid << " Failed to umap " << filename << ": " << strerror(eno) << std::endl;
      exit(1);
    }

    /* Update to the in-core buffer*/
    double *arr = (double *) base_addr;
    size_t array_size = umap_region_length/sizeof(double);

    for(size_t i=array_size/2; i < array_size; i++)
      arr[i] = tid*10000.0;
  

    if (umap_flush() < 0) {
      std::cerr << "Thread "<< tid << "Failed to flush cache to " << filename << std::endl;
      exit(1);
    }

    // open and load file separately to check flush has write back all dirty pages
    ifstream rf(filename, ios::in | ios::binary);
    if(!rf) {
      std::cout << "Cannot open file!" << std::endl;
      exit(1);
    }

    double arr_in[array_size];
    rf.read((char *) &arr_in[0], sizeof(uint64_t)*array_size);
    rf.close();
    bool is_correct = true;
    for(size_t i=0; i < array_size; i++) {
      if( arr_in[i]!=arr[i] ){
        printf("arr_in[%zu]=%lu != arr[%zu]=%lu\n", i, arr_in[i], i, arr[i] );
        is_correct = false;
      }
    }

    if (uunmap(base_addr, umap_region_length) < 0) {
      int eno = errno;
      std::cerr << "Thread "<< tid << " failed to uumap " << filename << ": " << strerror(eno) << std::endl;
      exit(1);
    }
    close(fd);

    if( is_correct )
      std::cout << "Thread "<< tid << " Passed " << "\n";
  }//end of parallel region

  return 0;
}
