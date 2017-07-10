/*
This file is part of UMAP.  For copyright information see the COPYRIGHT
file in the top level directory, or at
https://github.com/LLNL/umap/blob/master/COPYRIGHT
This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free
Software Foundation) version 2.1 dated February 1999.  This program is
distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the terms and conditions of the GNU Lesser General Public License
for more details.  You should have received a copy of the GNU Lesser General
Public License along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
#include <random>
#include <algorithm>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>    // optind
#include <errno.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "umap.h"
#include "umaptest.h"

static inline uint64_t getns(void)
{
  struct timespec ts;
  int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  assert(ret == 0);
  return (((uint64_t)ts.tv_sec) * 1000000000ULL) + ts.tv_nsec;
}

void runtest(uint64_t *region, int64_t rlen)
{
  static const uint64_t test_iterations = 1000000;
#pragma omp parallel
  {
    std::mt19937 gen(omp_get_thread_num());
    std::uniform_int_distribution<uint64_t> rnd_int(0, rlen-1);
    for (uint64_t i = 0; i < test_iterations; ++i) {
      uint64_t index = rnd_int(gen);
      if (region[index] != index) {
        fprintf(stderr, "%lu != %lu\n", index, region[index]);
        assert(0);
      }
    }
  }
}

void initdata(uint64_t *region, int64_t rlen)
{
  fprintf(stdout, "initdata: %p, %ld\n", region, rlen);
#pragma omp parallel for
  for(int64_t i=0; i < rlen; ++i)
    region[i] = i;
}

int main(int argc, char **argv)
{
  umt_optstruct_t options;
  long pagesize;
  int64_t totalbytes;
  uint64_t arraysize;
  void* base_addr;
  int fd;

  pagesize = umt_getpagesize();

  umt_getoptions(&options, argc, argv);

  totalbytes = options.numpages*pagesize;
  fd = umt_openandmap(&options, totalbytes, &base_addr);
 
  fprintf(stdout, "%lu pages, %lu threads\n", options.numpages, options.numthreads);

  omp_set_num_threads(options.numthreads);

  uint64_t *arr = (uint64_t *) base_addr; 
  arraysize = totalbytes/sizeof(int64_t);

  uint64_t start = getns();
  if ( !options.noinit ) {
    // init data
    initdata(arr, arraysize);
    fprintf(stdout, "Init took %f us\n", (double)(getns() - start)/1000000.0);
  }

  if ( !options.initonly ) {
    start = getns();
    runtest(arr, arraysize);
    fprintf(stdout, "Sort took %f us\n", (double)(getns() - start)/1000000.0);
  }

  umt_closeandunmap(&options, totalbytes, base_addr, fd);
  return 0;
}
