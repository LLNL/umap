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
# include <sys/time.h>
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

double microsecond()
{
  struct timeval tp;
  struct timezone tzp;
  int i;

  i = gettimeofday(&tp,&tzp);
  return ( (double) tp.tv_sec * 1.e6 + (double) tp.tv_usec );
}

void validate_file( const char* filename , size_t bytes ){

  std::cout << "Open "<< filename <<" separately to read content.\n";
  ifstream rf(filename, ios::in | ios::binary);
  if(!rf) {
    std::cout << "Cannot open file!" << std::endl;
    exit(1);
  }

  char *arr_in = (char *)malloc(bytes);
  rf.read( arr_in, bytes);
  rf.close();
  std::cout << "Read " << bytes << " bytes\n";

  double *a = (double*) arr_in;
  size_t array_length = bytes/sizeof(double);

  for(size_t i=0; i < array_length; i++) {
    if( a[i] != i * 1.0) {
      printf("\t a[%d] = %f \n", i, a[i]); 
    } 
  }

  free(arr_in);
}


int main(int argc, char **argv)
{
  printf("%s file_name num_bytes [region_pagesize] \n", argv[0]);
  if( argc != 3 && argc != 4 ) return 0;
  const char* filename_prefix = argv[1];
  uint64_t umap_region_length = atoll(argv[2]);
  uint64_t umap_pagesize = umapcfg_get_umap_page_size();
  uint64_t num_pages = (umap_region_length-1)/umap_pagesize + 1;
  umap_region_length = umap_pagesize * num_pages;
  std::cout << umap_pagesize << " bytes x " <<  num_pages << " pages = " << umap_region_length << " bytes\n";

  std::stringstream ss;
  ss << filename_prefix;
  std::string str = ss.str();
  const char* filename = str.c_str();
  int fd = open_prealloc_file(filename, umap_region_length);
  std::cout << "open_prealloc_file "<< filename << " of " << umap_region_length << " bytes\n";

  void* base_addr;
  if( argc > 3){
    base_addr = umap_variable(NULL, umap_region_length, PROT_READ|PROT_WRITE, UMAP_PRIVATE, fd, 0, atoi(argv[3]));
  }else{
    base_addr = umap(NULL, umap_region_length, PROT_READ|PROT_WRITE, UMAP_PRIVATE, fd, 0);
  }
  if ( base_addr == UMAP_FAILED ) {
    int eno = errno;
    std::cerr << " Failed to umap " << filename << ": " << strerror(eno) << std::endl;
    exit(1);
  }

  size_t array_length = umap_region_length / sizeof(double);
  double *a = (double*) base_addr;

  double t0 =  microsecond();
  #pragma omp parallel
  {
    if(omp_get_thread_num()==0)
      std::cout << omp_get_num_threads() << " OMP threads \n";

    #pragma omp for
    for (size_t i = 0; i<array_length; i++){
      a[i] = i * 1.0;
    }
  }
  double t1 =  microsecond();
  printf("Initialization in %.1f microseconds\n", (t1-t0));

  #pragma omp parallel for
  for (size_t i = 0; i<array_length; i++){
    if( a[i] != i * 1.0) {
      printf("\t a[%d] = %f \n", i, a[i]); 
      exit(1);
    }
  }
  printf("Validation is done\n");

  if (uunmap(base_addr, umap_region_length) < 0) {
    int eno = errno;
    std::cerr << "Failed to uumap " << filename << ": " << strerror(eno) << std::endl;
    exit(1);
  }
  std::cout << "Calling uunmap is done. \n";

  close(fd);
  std::cout << "file closed "<< filename << "\n";


  validate_file( filename, umap_region_length );
  std::cout << "validate_file finished \n";

  return 0;
}
