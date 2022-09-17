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
  std::cout << "open_prealloc_file "<< fname << "\n";

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
  printf("%s file_name num_bytes \n", argv[0]);
  if( argc != 3 && argc != 4 ) return 0;
  const char* filename = argv[1];
  uint64_t umap_region_length = atoll(argv[2]);
  uint64_t umap_pagesize = umapcfg_get_umap_page_size();
  uint64_t num_pages = (umap_region_length-1)/umap_pagesize + 1;
  umap_region_length = umap_pagesize * num_pages;
  std::cout << umap_pagesize << " bytes x " <<  num_pages << " pages = " << umap_region_length << " bytes\n";

  int fd = open_prealloc_file(filename, umap_region_length);

  void* base_addr;
  if( argc == 3 ){
    base_addr = umap(NULL, umap_region_length, PROT_READ|PROT_WRITE, UMAP_PRIVATE, fd, 0);
  }else{
    base_addr = umap_variable(NULL, umap_region_length, PROT_READ|PROT_WRITE, UMAP_PRIVATE, fd, 0, atoi(argv[3]));    
  }   
  if ( base_addr == UMAP_FAILED ) {
    int eno = errno;
    std::cerr << "Failed to umap " << filename << ": " << strerror(eno) << std::endl;
    return -1;
  }
  std::cout << "UMapped to "<< base_addr <<"\n";


  /* Update to array */
  uint64_t *arr = (uint64_t *) base_addr;
  size_t array_size = umap_region_length/sizeof(uint64_t);
#pragma omp parallel for
  for(size_t i=0; i < array_size; i++)
    arr[i] = (uint64_t)(i);
  std::cout << "Updated Array of "<< array_size <<" uint64_t\n";

  if (umap_flush() < 0) {
    std::cerr << "Failed to flush to " << filename << std::endl;
    exit(1);
  }
  std::cout << "umap_flush is done\n";

  // Check that updates have been flushed into the datastore
  std::cout << "Open the file separately to read content before calling uunmap \n";
  ifstream rf(filename, ios::in | ios::binary);
  if(!rf) {
    std::cout << "Cannot open file!" << std::endl;
    exit(1);
  }
  uint64_t *arr_in = (uint64_t*)malloc( sizeof(uint64_t)*array_size );
  rf.read((char *) &arr_in[0], sizeof(uint64_t)*array_size);
  rf.close();
  bool is_validated = true;
  for(size_t i=0; i < array_size; i++) {
    if( arr_in[i] != (uint64_t) i ) {
      printf("\t arr_in[%d] = %f \n", i, arr_in[i]);
      is_validated = false;
    } 
  }
  if(is_validated){
    std::cout << "Flush successed.\n";
  }else{
    std::cout << "Flush failed.\n";
  }

  // Unmap the region
  if (uunmap(base_addr, umap_region_length) < 0) {
    int eno = errno;
    std::cerr << "Failed to uumap " << filename << ": " << strerror(eno) << std::endl;
    exit(1);
  }else{
    std::cout << "Umapped is done.\n";
  }

  // finally, close the file
  close(fd);
  std::cout << "Passed\n";
  
  return 0;
}
