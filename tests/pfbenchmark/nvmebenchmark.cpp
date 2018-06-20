/* This file is part of UMAP.  For copyright information see the COPYRIGHT 
 * file in the top level directory, or at https://github.com/LLNL/umap/blob/master/COPYRIGHT 
 * This program is free software; you can redistribute it and/or modify it under 
 * the terms of the GNU Lesser General Public License (as published by the Free 
 * Software Foundation) version 2.1 dated February 1999.  This program is distributed in 
 * the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED 
 * WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * See the terms and conditions of the GNU Lesser General Public License for more details.  
 * You should have received a copy of the GNU Lesser General Public License along with 
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, 
 * Suite 330, Boston, MA 02111-1307 USA 
 */
// uffd sort benchmark

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <iostream>
#include <chrono>
#include <omp.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "umap.h"
#include "testoptions.h"
#include "PerFile.h"

using namespace std;
using namespace chrono;
static uint64_t pagesize;
static char* tmppagebuf;
static int fd;

void do_write_pages(uint64_t pages)
{
  *(uint64_t*)(tmppagebuf) = 12345;
#pragma omp parallel for
  for (uint64_t i = 0; i < pages; ++i) {
    if (pwrite(fd, tmppagebuf, pagesize, i*pagesize) < 0) {
      perror("pwrite");
      exit(1);
    }
  }
}

void do_read_pages(uint64_t pages)
{
#pragma omp parallel for
  for (uint64_t i = 0; i < pages; ++i) {
    if (pread(fd, tmppagebuf, pagesize, i*pagesize) < 0) {
      perror("pread");
      exit(1);
    }

    if ( *(uint64_t*)(tmppagebuf) != (uint64_t)12345 ) {
      cout << i << " " << *(uint64_t*)(tmppagebuf) << " != " << (uint64_t)(12345) << "\n";
      exit(1);
    }
  }
}

int main(int argc, char **argv)
{
  auto start_time = chrono::high_resolution_clock::now();
  auto end_time = chrono::high_resolution_clock::now();
  umt_optstruct_t options;
  umt_getoptions(&options, argc, argv);

  omp_set_num_threads(options.numthreads);

  pagesize = (uint64_t)umt_getpagesize();

  unlink(options.filename);
  fd = open(options.filename, O_RDWR | O_LARGEFILE | O_DIRECT | O_CREAT, S_IRUSR | S_IWUSR);

  if (fd == -1) {
    perror("open failed\n");
    exit(1);
  }

  if (posix_memalign((void**)&tmppagebuf, (uint64_t)512, pagesize)) {
    cerr << "ERROR: posix_memalign: failed\n";
    exit(1);
  }

  if (tmppagebuf == nullptr) {
    cerr << "Unable to allocate " << pagesize << " bytes for temporary buffer\n";
    exit(1);
  }


  //
  // Perform write test first.  This will also initialize the data
  //
  start_time = chrono::high_resolution_clock::now();
  do_write_pages(options.numpages);
  end_time = chrono::high_resolution_clock::now();
  cout  << chrono::duration_cast<chrono::nanoseconds>(end_time - start_time).count() / options.numpages << ",";

  start_time = chrono::high_resolution_clock::now();
  do_read_pages(options.numpages);
  end_time = chrono::high_resolution_clock::now();

  cout  << chrono::duration_cast<chrono::nanoseconds>(end_time - start_time).count() / options.numpages << "\n";

  return 0;
}
