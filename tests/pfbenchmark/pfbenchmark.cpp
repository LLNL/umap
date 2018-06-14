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

#include "umap.h"
#include "testoptions.h"
#include "PerFile.h"

using namespace std;
using namespace chrono;
static bool no_io = false;
static bool usemmap = false;
static uint64_t pagesize;
static uint64_t page_step;
static uint64_t* array;

void do_write_pages(uint64_t* array, uint64_t page_step, uint64_t pages)
{
#pragma omp parallel for
  for (uint64_t i = 0; i < pages; ++i)
    array[i * page_step] = (i * page_step);
}

void do_read_pages(uint64_t* array, uint64_t page_step, uint64_t pages)
{
#pragma omp parallel for
  for (uint64_t i = 0; i < pages; ++i) {
    if ( array[i * page_step] != (i * page_step) && no_io == false ) {
      cout << "array[" << i * page_step << "]: (" << array[i*page_step] << ") != " << i * page_step << "\n";
    }
  }
}

void print_stats( void )
{
  if (!usemmap) {
    struct umap_cfg_stats s;
    umap_cfg_get_stats(array, &s);

    cout << s.dirty_evicts << " Dirty Evictions\n";
    cout << s.clean_evicts << " Clean Evictions\n";
    cout << s.evict_victims << " Victims\n";
    cout << s.wp_messages << " WP Faults\n";
    cout << s.read_faults << " Read Faults\n";
    cout << s.write_faults << " Write Faults\n";
    cout << s.sigbus << " SIGBUS Signals\n";
    cout << s.stuck_wp << " Stuck WP Workarounds\n";
    cout << s.dropped_dups << " Dropped Duplicates\n";
  }
}

int main(int argc, char **argv)
{
  auto start_time = chrono::high_resolution_clock::now();
  auto end_time = chrono::high_resolution_clock::now();
  umt_optstruct_t options;
  umt_getoptions(&options, argc, argv);

  options.noinit = 0;
  options.initonly = 0;
  no_io = (options.noio == 1);
  usemmap = (options.usemmap == 1);

  omp_set_num_threads(options.numthreads);

  pagesize = (uint64_t)umt_getpagesize();
  page_step = pagesize/sizeof(uint64_t);
  array = (uint64_t*)PerFile_openandmap(&options, pagesize * options.numpages);

  cout << ((options.usemmap == 1) ? "mmap" : "umap") << ","
      << ((options.noio == 1) ? "no" : "yes") << ","
      << options.numthreads << ","
      << options.uffdthreads << ",";

  //
  // Perform write test first.  This will also initialize the data
  //
  umap_cfg_flush_buffer(array);
  umap_cfg_reset_stats(array);

  start_time = chrono::high_resolution_clock::now();
  do_write_pages(array, page_step, options.numpages);
  end_time = chrono::high_resolution_clock::now();
  cout  << chrono::duration_cast<chrono::nanoseconds>(end_time - start_time).count() / options.numpages << ",";

  start_time = chrono::high_resolution_clock::now();
  umap_cfg_flush_buffer(array);
  end_time = chrono::high_resolution_clock::now();
  cout  << chrono::duration_cast<chrono::nanoseconds>(end_time - start_time).count() / options.numpages << ",";

  //print_stats();

  //
  // Perform WP test.  This will also initialize the data
  //
  umap_cfg_flush_buffer(array);
  umap_cfg_reset_stats(array);

  do_read_pages(array, page_step, options.numpages);

  start_time = chrono::high_resolution_clock::now();
  do_write_pages(array, page_step, options.numpages);
  end_time = chrono::high_resolution_clock::now();
  cout  << chrono::duration_cast<chrono::nanoseconds>(end_time - start_time).count() / options.numpages << ",";
  //print_stats();

  umap_cfg_flush_buffer(array);
  umap_cfg_reset_stats(array);

  start_time = chrono::high_resolution_clock::now();
  do_read_pages(array, page_step, options.numpages);
  end_time = chrono::high_resolution_clock::now();

  cout  << chrono::duration_cast<chrono::nanoseconds>(end_time - start_time).count() / options.numpages << "\n";
  //print_stats();

  PerFile_closeandunmap(&options, pagesize * options.numpages, array);

  return 0;
}
