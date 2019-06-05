//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

/*
 * It is a simple example showing how an application may map to a a file,
 * Initialize the file with data, sort the data, then verify that sort worked
 * correctly.
 */
#include <iostream>
#include <parallel/algorithm>
#include <fcntl.h>
#include <omp.h>
#include <cstdio>
#include <cstring>
#include "errno.h"
#include "umap/umap.h"

using namespace std;

void initialize_and_sort_file( const char* fname, uint64_t arraysize, uint64_t totalbytes )
{
  if ( unlink(fname) ) {
    int eno = errno;
    if ( eno != ENOENT ) {
      cerr << "Failed to unlink " << fname << ": " 
        << strerror(eno) << " Errno=" << eno << endl;
    }
  }

  int fd = open(fname, O_RDWR | O_LARGEFILE | O_DIRECT | O_CREAT, S_IRUSR | S_IWUSR);
  if ( fd == -1 ) {
    int eno = errno;
    cerr << "Failed to create " << fname << ": " << strerror(eno) << endl;
    return;
  }

  // If we are initializing, attempt to pre-allocate disk space for the file.
  try {
    int x;
    if ( ( x = posix_fallocate(fd, 0, totalbytes) != 0 ) ) {
      int eno = errno;
      cerr << "Failed to pre-allocate " << fname << ": " << strerror(eno) << endl;
      return;
    }
  } catch(const std::exception& e) {
    std::cerr << "posix_fallocate: " << e.what() << std::endl;
    return;
  } catch(...) {
    int eno = errno;
    cerr << "Failed to pre-allocate " << fname << ": " << strerror(eno) << endl;
    return;
  }

  void* base_addr = umap(NULL, totalbytes, PROT_READ|PROT_WRITE, UMAP_PRIVATE, fd, 0);
  if ( base_addr == UMAP_FAILED ) {
    int eno = errno;
    cerr << "Failed to umap " << fname << ": " << strerror(eno) << endl;
    return;
  }

  uint64_t *arr = (uint64_t *) base_addr;

  cout << "Initializing Array\n";

#pragma omp parallel for
  for(uint64_t i=0; i < arraysize; ++i)
    arr[i] = (uint64_t) (arraysize - i);

  cout << "Sorting Data\n";
  __gnu_parallel::sort(arr, &arr[arraysize], std::less<uint64_t>(), __gnu_parallel::quicksort_tag());


  if (uunmap(base_addr, totalbytes) < 0) {
    int eno = errno;
    cerr << "Failed to uumap " << fname << ": " << strerror(eno) << endl;
    return;
  }
  close(fd);
}

void verify_sortfile( const char* fname, uint64_t arraysize, uint64_t totalbytes )
{
  int fd = open(fname, O_RDWR | O_LARGEFILE | O_DIRECT, S_IRUSR | S_IWUSR);
  if ( fd == -1 ) {
    cerr << "Failed to create " << fname << endl;
    return;
  }

  void* base_addr = umap(NULL, totalbytes, PROT_READ|PROT_WRITE, UMAP_PRIVATE, fd, 0);
  if ( base_addr == UMAP_FAILED ) {
    cerr << "umap failed\n";
    return;
  }
  uint64_t *arr = (uint64_t *) base_addr;

  cout << "Verifying Data with\n";

#pragma omp parallel for
  for(uint64_t i = 0; i < arraysize; ++i)
    if (arr[i] != (i+1)) {
      cerr << "Data miscompare\n";
      i = arraysize;
    }

  if (uunmap(base_addr, totalbytes) < 0) {
    cerr << "uunamp failed\n";
    return;
  }
  close(fd);
}

int main(int argc, char **argv)
{
  const char* filename = argv[1];

  // Optional: Make umap's pages size double the default system page size
  //
  // Use UMAP_PAGE_SIZE environment variable to set page size for umap
  //
  uint64_t psize = umapcfg_get_umap_page_size();

  const uint64_t pagesInTest = 64;
  const uint64_t elemPerPage = psize / sizeof(uint64_t);

  const uint64_t arraySize = elemPerPage * pagesInTest;
  const uint64_t totalBytes = arraySize * sizeof(uint64_t);

  // Optional: Set umap's buffer to half the number of pages we need so that
  //           we may simulate an out-of-core experience
  //
  // Use UMAP_BUFSIZE environment variable to set number of pages in buffer
  //

  initialize_and_sort_file(filename, arraySize, totalBytes);
  verify_sortfile(filename, arraySize, totalBytes);
  return 0;
}
