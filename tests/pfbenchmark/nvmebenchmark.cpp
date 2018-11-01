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
 *
 * This program is a benchmark for NVME device I/O bandwidth which provides the average
 * time in nanoseconds for performing the following I/O operations:
 *
 * 1) Page (4K) writes to a file on the NVME device
 * 2) Page (4K) reads from a file on the NVME device
 *
 * A number of threads may be specified on the command line to enable concurrent I/O
 * access within the file.  Further, the file may be accessed sequentially (default)
 * or randomly (if "--shuffle" command line option is specified).
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <iostream>
#include <chrono>
#include <omp.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <sstream>
#include <vector>
#include <random>
#include <algorithm>
#include <iterator>

#include "umap/umap.h"
#include "../utility/commandline.hpp"

using namespace std;
using namespace chrono;
static uint64_t pagesize;
static uint64_t pages_to_access;
static char** tmppagebuf; // One per thread
static int fd;
static utility::umt_optstruct_t options;
vector<uint64_t> shuffled_indexes;

void do_write_pages(uint64_t pages)
{
#pragma omp parallel for
  for (uint64_t i = 0; i < pages; ++i) {
    uint64_t myidx = shuffled_indexes[i];
    uint64_t* ptr = (uint64_t*)tmppagebuf[omp_get_thread_num()];
    *ptr = myidx * pagesize/sizeof(uint64_t);
    if (pwrite(fd, ptr, pagesize, myidx*pagesize) < 0) {
      perror("pwrite");
      exit(1);
    }
  }
}

void do_read_pages(uint64_t pages)
{
#pragma omp parallel for
  for (uint64_t i = 0; i < pages; ++i) {
    uint64_t myidx = shuffled_indexes[i];
    uint64_t* ptr = (uint64_t*)tmppagebuf[omp_get_thread_num()];
    if (pread(fd, ptr, pagesize, myidx*pagesize) < 0) {
      perror("pread");
      exit(1);
    }
    if ( *ptr != myidx * pagesize/sizeof(uint64_t) ) {
      cout << i << " " << myidx << " " << *ptr << " != " << (myidx * pagesize/sizeof(uint64_t)) << "\n";
      exit(1);
    }
  }
}

int read_pages(int argc, char **argv)
{
  fd = open(options.filename, O_RDWR | O_LARGEFILE | O_DIRECT);

  if (fd == -1) {
    perror("open failed\n");
    exit(1);
  }

  auto start_time = chrono::high_resolution_clock::now();
  do_read_pages(pages_to_access);
  auto end_time = chrono::high_resolution_clock::now();

  cout << "nvme,"
      << "+IO,"
      << (( options.shuffle == 1) ? "shuffle" : "seq") << ","
      << "read,"
      << options.numthreads << ","
      << 0 << ","
      << chrono::duration_cast<chrono::nanoseconds>(end_time - start_time).count() / pages_to_access << "\n";

  close(fd);
  return 0;
}

int write_pages(int argc, char **argv)
{
  if ( !options.noinit ) {
    cout << "Removing " << options.filename << "\n";
    unlink(options.filename);
    cout << "Creating " << options.filename << "\n";
    fd = open(options.filename, O_RDWR | O_LARGEFILE | O_DIRECT | O_CREAT, S_IRUSR | S_IWUSR);
  }
  else {
    fd = open(options.filename, O_RDWR | O_LARGEFILE | O_DIRECT);
  }

  if (fd == -1) {
    perror("open failed\n");
    exit(1);
  }

  auto start_time = chrono::high_resolution_clock::now();
  do_write_pages(pages_to_access);
  auto end_time = chrono::high_resolution_clock::now();

  cout << "nvme,"
      << "+IO,"
      << (( options.shuffle == 1) ? "shuffle" : "seq") << ","
      << "write,"
      << options.numthreads << ","
      << 0 << ","
      << chrono::duration_cast<chrono::nanoseconds>(end_time - start_time).count() / pages_to_access << "\n";

  close(fd);
  return 0;
}

int main(int argc, char **argv)
{
  int rval = 0;
  std::random_device rd;
  std::mt19937 g(rd());

  umt_getoptions(&options, argc, argv);

  for (uint64_t i = 0; i < options.numpages; ++i)
    shuffled_indexes.push_back(i);

  pages_to_access = options.pages_to_access ? options.pages_to_access : options.numpages;

  if ( options.shuffle )
    std::shuffle(shuffled_indexes.begin(), shuffled_indexes.end(), g);

  omp_set_num_threads(options.numthreads);
  pagesize = (uint64_t)utility::umt_getpagesize();

  tmppagebuf = (char**)calloc(options.numthreads, sizeof(char*));

  for (int i = 0; i < options.numthreads; ++i) {
    if (posix_memalign((void**)&tmppagebuf[i], (uint64_t)512, pagesize)) {
      cerr << "ERROR: posix_memalign: failed\n";
      exit(1);
    }

    if (tmppagebuf[i] == nullptr) {
      cerr << "Unable to allocate " << pagesize << " bytes for temporary buffer\n";
      exit(1);
    }
  }

  if (strcmp(argv[0], "nvmebenchmark-write") == 0)
    rval = write_pages(argc, argv);
  else if (strcmp(argv[0], "nvmebenchmark-read") == 0)
    rval = read_pages(argc, argv);

  return rval;
}
