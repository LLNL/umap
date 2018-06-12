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

void do_write_pages(uint64_t* array, uint64_t page_step, uint64_t pages, uint64_t val)
{
#pragma omp parallel for
  for (uint64_t i = 0; i < pages; ++i)
    array[i * page_step] = val;
}

void do_read_pages(uint64_t* array, uint64_t page_step, uint64_t pages)
{
#pragma omp parallel for
  for (uint64_t i = 0; i < pages; ++i) {
    if (array[i * page_step] == 0x12345678) {
      cout << "Hello!\n";
    }
  }
}

void run_write_test(umt_optstruct_t* options)
{
  uint64_t pagesize = (uint64_t)umt_getpagesize();
  uint64_t page_step = pagesize/sizeof(uint64_t);
  uint64_t* array = (uint64_t*)PerFile_openandmap(options, pagesize * options->numpages);
 
  auto start_time = chrono::high_resolution_clock::now();
  do_write_pages(array, page_step, options->numpages, 22);
  auto end_time = chrono::high_resolution_clock::now();

  cout  << chrono::duration_cast<chrono::nanoseconds>(end_time - start_time).count() / options->numpages
        << " nanoseconds per Write Page Fault\n";

  PerFile_closeandunmap(options, pagesize * options->numpages, array);
}

void run_read_test(umt_optstruct_t* options)
{
  uint64_t pagesize = (uint64_t)umt_getpagesize();
  uint64_t page_step = pagesize/sizeof(uint64_t);
  uint64_t* array = (uint64_t*)PerFile_openandmap(options, pagesize * options->numpages);
 
  auto start_time = chrono::high_resolution_clock::now();
  do_read_pages(array, page_step, options->numpages);
  auto end_time = chrono::high_resolution_clock::now();

  cout  << chrono::duration_cast<chrono::nanoseconds>(end_time - start_time).count() / options->numpages
        << " nanoseconds per Read Page Fault " << endl;

  PerFile_closeandunmap(options, pagesize * options->numpages, array);
}

void run_wp_test(umt_optstruct_t* options)
{
  uint64_t pagesize = (uint64_t)umt_getpagesize();
  uint64_t page_step = pagesize/sizeof(uint64_t);
  uint64_t* array = (uint64_t*)PerFile_openandmap(options, pagesize * options->numpages);
 
  do_read_pages(array, page_step, options->numpages);

  auto start_time = chrono::high_resolution_clock::now();
  do_write_pages(array, page_step, options->numpages, 0x33);
  auto end_time = chrono::high_resolution_clock::now();

  cout  << chrono::duration_cast<chrono::nanoseconds>(end_time - start_time).count() / options->numpages
        << " nanoseconds per WP Page Fault\n";

  PerFile_closeandunmap(options, pagesize * options->numpages, array);
}

int main(int argc, char **argv)
{
  umt_optstruct_t options;

  umt_getoptions(&options, argc, argv);

  omp_set_num_threads(options.numthreads);

  cout << "Running with " << options.numthreads << " threads\n";
  //run_write_test(&options);

  run_read_test(&options);

  // run_wp_test(&options);

  return 0;
}
