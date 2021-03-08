//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

/*
 * It is a simple example based on STREAM benchmark ADD test that 
 * perfors element-wise simmation of two arrays. 
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

#define ELEMENT_T double

using namespace std;

int
init_file( const char* fname, uint64_t totalbytes)
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

  ELEMENT_T *arr_a = (ELEMENT_T *) mmap(NULL, totalbytes, PROT_WRITE, MAP_SHARED, fd, 0);
  if(arr_a==MAP_FAILED)
    std::cout<<"Failed to mmap "<< totalbytes <<" bytes"<<std::endl;

  size_t num_elements = totalbytes/sizeof(ELEMENT_T);
#pragma omp parallel for
  for(size_t i=0; i<num_elements; i++)
    arr_a[i] = (ELEMENT_T) i;

  munmap(arr_a, totalbytes);
  close(fd);
  
  return 0;
}

int
main(int argc, char **argv)
{
  const char* filename_prefix = argv[1];
  const int num_umap_pages = atoi(argv[2]);
  
  uint64_t umap_pagesize = umapcfg_get_umap_page_size();
  const uint64_t umap_region_length = num_umap_pages * umap_pagesize;
  std::cout << "umap_region "  << umap_region_length << " (bytes) \n";

  
  std::stringstream ss;
  ss << filename_prefix << "/arr_a";
  std::string str0 = ss.str();
  const char* file0 = str0.c_str();
  init_file(file0, umap_region_length);

  ss.str("");
  ss << filename_prefix << "/arr_b";
  std::string str1 = ss.str();
  const char* file1 = str1.c_str();
  init_file(file1, umap_region_length);
  

  int fd0 = open(file0, O_RDONLY, S_IRUSR | S_IWUSR);
  ELEMENT_T* arr_a = (ELEMENT_T*) umap(NULL, umap_region_length, PROT_READ, UMAP_PRIVATE, fd0, 0);
  if ( arr_a == UMAP_FAILED ) {
    int eno = errno;
    std::cerr << " Failed to umap " << file0 << ": " << strerror(eno) << std::endl;
    exit(1);
  }

  int fd1 = open(file1, O_RDONLY, S_IRUSR | S_IWUSR);
  ELEMENT_T* arr_b = (ELEMENT_T*) umap(NULL, umap_region_length, PROT_READ, UMAP_PRIVATE, fd1, 0);
  if ( arr_a == UMAP_FAILED ) {
    int eno = errno;
    std::cerr << " Failed to umap " << file1 << ": " << strerror(eno) << std::endl;
    exit(1);
  }


  size_t num_elements = umap_region_length/sizeof(ELEMENT_T);
  bool is_results_valid = true;
#pragma omp parallel for
  for(size_t i=0; i<num_elements; i++){
    ELEMENT_T c = arr_a[i] + arr_b[i];
    if( c != (ELEMENT_T)(i+i) ){
      std::cout << "Error at ["<<i<<"]: " << c << " \n";
      is_results_valid = false;
    }
  }
  if(is_results_valid)
    std::cout << "Results passed validation \n";
  else
    std::cout << "Results failed validation \n";
  
  /*  close(fd0);
  uunmap(arr_a, umap_region_length);

  close(fd1);
  uunmap(arr_b, umap_region_length);
  */
  
  return 0;
}
