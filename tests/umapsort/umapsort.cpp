//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <iostream>
#include <string>
#include <sstream>
#include <cassert>
#include <random>
#include <string>
#include <vector>
#include <parallel/algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <omp.h>

#include "umap/umap.h"
#include "commandline.hpp"
#include "umap_file.hpp"
#include "time.hpp"

using namespace std;

bool sort_ascending = true;

void initdata(uint64_t *region, uint64_t rlen) {
  fprintf(stderr, "initdata: %p, from %lu to %lu\n", region, (rlen), (rlen - rlen));
#pragma omp parallel for
  for(uint64_t i=0; i < rlen; ++i)
    region[i] = (uint64_t) (rlen - i);
}

uint64_t dump_page( uint64_t* region, uint64_t index )
{
  uint64_t pageSize = (uint64_t)utility::umt_getpagesize();
  uint64_t elemsPerPage = pageSize / sizeof(uint64_t);
  uint64_t pageNumber = index / elemsPerPage;
  uint64_t pageStartIndex = pageNumber*elemsPerPage;
  uint64_t* page = &region[pageStartIndex];

  fprintf(stderr, "Data miscompare in page %lu\n", pageNumber);

  for ( uint64_t i = pageStartIndex; i < (pageStartIndex + elemsPerPage); ++i ) {
    if ( i == index )
      fprintf(stderr, "**%8lu %8lu\n", i, region[i]);
    else
      fprintf(stderr, "  %8lu %8lu\n", i, region[i]);
  }

  return pageStartIndex + elemsPerPage; // got to next page
}

void validatedata(uint64_t *region, uint64_t rlen) {
  if (sort_ascending == true) {
// #pragma omp parallel for
    for(uint64_t i = 0; i < rlen; ++i) {
        if (region[i] != (i+1)) {
            fprintf(stderr, "Worker %d found an error at index %lu, %lu != lt %lu!\n",
                            omp_get_thread_num(), i, region[i], i+1);

            if (i < 3) {
                fprintf(stderr, "Context ");
                for (int j=0; j < 7; j++) {
                    fprintf(stderr, "%lu ", region[j]);
                }
                fprintf(stderr, "\n");
            }
            else if (i > (rlen-4)) {
                fprintf(stderr, "Context ");
                for (uint64_t j=rlen-8; j < rlen; j++) {
                    fprintf(stderr, "%lu ", region[j]);
                }
                fprintf(stderr, "\n");
            }
            else {
                fprintf(stderr,
                    "Context i-3 i-2 i-1 i i+1 i+2 i+3:%lu %lu %lu %lu %lu %lu %lu\n",
                    region[i-3], region[i-2], region[i-1], region[i], region[i+1], region[i+2], region[i+3]);
            }
            i = dump_page( region, i ) - 1;
        }
    }
  }
  else {
// #pragma omp parallel for
    for(uint64_t i = 0; i < rlen; ++i) {
        if(region[i] != (rlen - i)) {
            fprintf(stderr, "Worker %d found an error at index %lu, %lu != %lu!\n",
                            omp_get_thread_num(), i, region[i], (rlen - i));

            if (i < 3) {
                fprintf(stderr, "Context ");
                for (int j=0; j < 7; j++) {
                    fprintf(stderr, "%lu ", region[j]);
                }
                fprintf(stderr, "\n");
            }
            else if (i > (rlen-4)) {
                fprintf(stderr, "Context ");
                for (uint64_t j=rlen-8; j < rlen; j++) {
                    fprintf(stderr, "%lu ", region[j]);
                }
                fprintf(stderr, "\n");
            }
            else {
                fprintf(stderr,
                    "Context i-3 i-2 i-1 i i+1 i+2 i+3:%lu %lu %lu %lu %lu %lu %lu\n",
                    region[i-3], region[i-2], region[i-1], region[i], region[i+1], region[i+2], region[i+3]);
            }
            exit(1);
        }
    }
  }
}

int main(int argc, char **argv)
{
  utility::umt_optstruct_t options;
  uint64_t pagesize;
  uint64_t totalbytes;
  uint64_t arraysize;
  void* base_addr;

  auto start = utility::elapsed_time_sec();
  pagesize = (uint64_t)utility::umt_getpagesize();

  umt_getoptions(&options, argc, argv);

  omp_set_num_threads(options.numthreads);

  totalbytes = options.numpages*pagesize;
  base_addr = utility::map_in_file(options.filename, options.initonly, options.noinit, options.usemmap, totalbytes);
  if (base_addr == nullptr)
    return -1;

  fprintf(stderr, "umap INIT took %f seconds\n", utility::elapsed_time_sec(start));
  fprintf(stderr, "%lu pages, %lu bytes, %lu threads\n", options.numpages, totalbytes, options.numthreads);

  uint64_t *arr = (uint64_t *) base_addr;
  arraysize = totalbytes/sizeof(uint64_t);

  start = utility::elapsed_time_sec();
  if ( !options.noinit ) {
    // init data
    initdata(arr, arraysize);
    fprintf(stderr, "INIT took %f seconds\n", utility::elapsed_time_sec(start));
  }

  if ( !options.initonly )
  {
    start = utility::elapsed_time_sec();
    sort_ascending = (arr[0] != 1);

    if (sort_ascending == true) {
      printf("Sorting in Ascending Order\n");
      __gnu_parallel::sort(arr, &arr[arraysize], std::less<uint64_t>(), __gnu_parallel::quicksort_tag());
    }
    else {
      printf("Sorting in Descending Order\n");
      __gnu_parallel::sort(arr, &arr[arraysize], std::greater<uint64_t>(), __gnu_parallel::quicksort_tag());
    }

    fprintf(stderr, "Sort took %f seconds\n", utility::elapsed_time_sec(start));

    start = utility::elapsed_time_sec();
    validatedata(arr, arraysize);
    fprintf(stderr, "Validate took %f seconds\n", utility::elapsed_time_sec(start));
  }

  start = utility::elapsed_time_sec();
  utility::unmap_file(options.usemmap, totalbytes, base_addr);
  fprintf(stderr, "umap TERM took %f seconds\n", utility::elapsed_time_sec(start));

  return 0;
}
