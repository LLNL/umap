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
 * This program is a benchmark for UMAP page fault handling.  The specied backing file
 * for UMAP is accessed a page at a time indirectly by accessing pages that have been
 * mapped to the file.  Read accesses to memory will determine the average nanosecond
 * cost for servicing a READ PAGE FAULT and write accesses to memory will determine
 * the average cost of WRITE PAGE FAULTs.
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
#include <string.h>
#include <vector>
#include <random>
#include <algorithm>
#include <iterator>

#include "umap/umap.h"
#include "../utility/umap_file.hpp"
#include "../utility/commandline.hpp"

using namespace std;
using namespace chrono;
static bool usemmap = false;
static uint64_t pagesize;
static uint64_t page_step;
static uint64_t* glb_array;
static utility::umt_optstruct_t options;
static uint64_t pages_to_access;
vector<uint64_t> shuffled_indexes;

void do_write_pages(uint64_t page_step, uint64_t pages)
{
#pragma omp parallel for
  for (uint64_t i = 0; i < pages; ++i) {
    uint64_t myidx = shuffled_indexes[i];
    glb_array[myidx * page_step] = (myidx * page_step);
  }
}

uint64_t do_read_pages(uint64_t page_step, uint64_t pages)
{
  uint64_t x;

  // Weird logic to make sure that compiler doesn't optimize out our read of glb_array[i]
#pragma omp parallel for
  for (uint64_t i = 0; i < pages; ++i) {
    uint64_t myidx = shuffled_indexes[i];
    x = glb_array[myidx * page_step];

    if (x != (myidx * page_step)) {
      cout << __FUNCTION__ << "glb_array[" << myidx * page_step << "]: (" << glb_array[myidx*page_step] << ") != " << myidx * page_step << "\n";
      exit(1);
    }
  }

  return x;
}

uint64_t do_read_modify_write_pages(uint64_t page_step, uint64_t pages)
{
  uint64_t x;

  // Weird logic to make sure that compiler doesn't optimize out our read of glb_array[i]
#pragma omp parallel for
  for (uint64_t i = 0; i < pages; ++i) {
    uint64_t myidx = shuffled_indexes[i];
    x = glb_array[myidx * page_step];

    if (x != (myidx * page_step)) {
      cout << __FUNCTION__ << "glb_array[" << myidx * page_step << "]: (" << x << ") != " << myidx * page_step << "\n";
      exit(1);
    }
    else {
      glb_array[myidx * page_step] = (myidx * page_step);
      x++;
    }
  }
  return x;
}

void print_stats( void )
{
  if (!usemmap) {
    struct umap_cfg_stats s;
    umap_cfg_get_stats(glb_array, &s);

    //cout << s.dirty_evicts << " Dirty Evictions\n";
    //cout << s.clean_evicts << " Clean Evictions\n";
    //cout << s.evict_victims << " Victims\n";
    //cout << s.wp_messages << " WP Faults\n";
    //cout << s.read_faults << " Read Faults\n";
    //cout << s.write_faults << " Write Faults\n";
    if (s.sigbus)
      cout << s.sigbus << " SIGBUS Signals\n";
    if (s.stuck_wp)
      cout << s.stuck_wp << " Stuck WP Workarounds\n";
    //cout << s.dropped_dups << " Dropped Duplicates\n";
  }
}

int read_test(int argc, char **argv)
{
  auto start_time = chrono::high_resolution_clock::now();
  do_read_pages(page_step, pages_to_access);
  auto end_time = chrono::high_resolution_clock::now();

  cout << ((options.usemmap == 1) ? "mmap" : "umap") << ","
      << (( options.shuffle == 1) ? "shuffle" : "seq") << ","
      << "read,"
      << options.numthreads << ","
      << options.uffdthreads << ","
      << chrono::duration_cast<chrono::nanoseconds>(end_time - start_time).count() / pages_to_access << "\n";

  return 0;
}

int write_test(int argc, char **argv)
{
  auto start_time = chrono::high_resolution_clock::now();
  do_write_pages(page_step, pages_to_access);
  auto end_time = chrono::high_resolution_clock::now();

  cout << ((options.usemmap == 1) ? "mmap" : "umap") << ","
      << (( options.shuffle == 1) ? "shuffle" : "seq") << ","
      << "write,"
      << options.numthreads << ","
      << options.uffdthreads << ","
      << chrono::duration_cast<chrono::nanoseconds>(end_time - start_time).count() / pages_to_access << "\n";

  return 0;
}

int read_modify_write_test(int argc, char **argv)
{
  auto start_time = chrono::high_resolution_clock::now();
  auto end_time = chrono::high_resolution_clock::now();

  start_time = chrono::high_resolution_clock::now();
  do_read_modify_write_pages(page_step, pages_to_access);
  end_time = chrono::high_resolution_clock::now();

  cout << ((options.usemmap == 1) ? "mmap" : "umap") << ","
      << (( options.shuffle == 1) ? "shuffle" : "seq") << ","
      << "rmw,"
      << options.numthreads << ","
      << options.uffdthreads << ","
      << chrono::duration_cast<chrono::nanoseconds>(end_time - start_time).count() / pages_to_access << "\n";

  return 0;
}

int main(int argc, char **argv)
{
  int rval = -1;
  std::random_device rd;
  std::mt19937 g(rd());

  umt_getoptions(&options, argc, argv);

  for (uint64_t i = 0; i < options.numpages; ++i)
    shuffled_indexes.push_back(i);

  pages_to_access = options.pages_to_access ? options.pages_to_access : options.numpages;

  if ( options.shuffle )
    std::shuffle(shuffled_indexes.begin(), shuffled_indexes.end(), g);

  options.initonly = 0;
  usemmap = (options.usemmap == 1);
  omp_set_num_threads(options.numthreads);
  pagesize = (uint64_t)utility::umt_getpagesize();
  page_step = pagesize/sizeof(uint64_t);

  glb_array = (uint64_t*) utility::map_in_file(options.filename, options.initonly,
      options.noinit, options.usemmap, pagesize * options.numpages);

  if (strcmp(argv[0], "pfbenchmark-read") == 0)
    rval = read_test(argc, argv);
  else if (strcmp(argv[0], "pfbenchmark-write") == 0)
    rval = write_test(argc, argv);
  else if (strcmp(argv[0], "pfbenchmark-readmodifywrite") == 0)
    rval = read_modify_write_test(argc, argv);
  else
    cerr << "Unknown test mode " << argv[0] << "\n";

  print_stats();
  utility::unmap_file(options.usemmap, pagesize * options.numpages, glb_array);
  return rval;
}
