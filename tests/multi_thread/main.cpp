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
  int fd = open(fname, O_RDWR | O_LARGEFILE | O_DIRECT | O_CREAT, S_IRUSR | S_IWUSR);
  if ( fd == -1 ) {
    int eno = errno;
    std::cerr << "Failed to create " << fname << ": " << strerror(eno) << std::endl;
    exit(1);
  }

  off_t fsize = lseek(fd, 0, SEEK_END);
  std::cout << "File size " <<  fsize << " bytes\n";
  if( fsize>=totalbytes ) return fd;

  std::cout << "Extend File size to " <<  totalbytes << " bytes\n";
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
  printf("%s file_name num_bytes\n", argv[0]);
  if( argc != 3 ) return 0;
  const char* filename_prefix = argv[1];
  uint64_t umap_region_length = atoll(argv[2]);
  uint64_t umap_pagesize = umapcfg_get_umap_page_size();
  uint64_t num_pages = (umap_region_length-1)/umap_pagesize + 1;
  umap_region_length = umap_pagesize * num_pages;
  std::cout << umap_pagesize << " bytes x " <<  num_pages << " pages = " << umap_region_length << " bytes\n";

int num_passed = 0;
int num_threads = 0;
#pragma omp parallel reduction(+:num_passed, num_threads)
  {
    int tid = omp_get_thread_num();
    num_threads ++;
    std::stringstream ss;
    ss << filename_prefix << "_t" << tid;
    std::string str = ss.str();
    const char* filename = str.c_str();
    int fd = open_prealloc_file(filename, umap_region_length);

    void* base_addr = umap(NULL, umap_region_length, PROT_READ|PROT_WRITE, UMAP_PRIVATE, fd, 0);
    if ( base_addr == UMAP_FAILED ) {
      int eno = errno;
      std::cerr << "Thread "<< tid << " Failed to umap " << filename << ": " << strerror(eno) << std::endl;
      exit(1);
    }
    std::cout << "Thread "<< tid << " umapped base_addr at "<< base_addr <<"\n";


    /* Update to the in-core buffer*/
    double *arr = (double *) base_addr;
    size_t array_size = umap_region_length/sizeof(double);
    for(size_t i=0; i < array_size; i++)
      arr[i] = i * 1.0 + tid * 0.000001;
    std::cout << "Thread "<< tid << " Update Array of "<< array_size <<" double\n";
  
    /* Flushed by multiple threads */
    if (umap_flush() < 0) {
      std::cerr << "Thread "<< tid << "Failed to flush cache to " << filename << std::endl;
      exit(1);
    }
    std::cout << "Thread "<< tid << " umap_flush done\n";

    /* Read in the datastore files to validate */
    std::cout << "Thread "<< tid << " open "<< filename <<" separately to read content.\n";
    ifstream rf(filename, ios::in | ios::binary);
    if(!rf) {
      std::cout << "Cannot open file " << filename << std::endl;
      exit(1);
    }

    char *arr_in = (char *)malloc(umap_region_length);
    rf.read( arr_in, umap_region_length);
    rf.close();

    double *a = (double*) arr_in;
    size_t array_length = umap_region_length/sizeof(double);

    for(size_t i=0; i < array_length; i++) {
      if( a[i] != (i * 1.0 + tid * 0.000001) ) {
        printf("\t Thread %d: a[%ld] = %.6f \n", tid, i, a[i]);
        break;
        exit(1);        
      } 
    }
    free(arr_in);

    if (uunmap(base_addr, umap_region_length) < 0) {
      int eno = errno;
      std::cerr << "Failed to uumap " << filename << ": " << strerror(eno) << std::endl;
      exit(1);
    }
    std::cout << "Thread "<< tid << " finished uunmap \n";

    close(fd);
    std::cout << "Thread "<< tid << " closed "<< filename << "\n";
    num_passed = 1;

  } // end of parallel region

  if(num_passed==num_threads){
    std::cout << "Pass \n";
  }else{
    std::cout << num_passed << " / " << num_threads << "\n";
  }
  return 0;
}
