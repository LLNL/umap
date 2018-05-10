/* This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
// uffd sort benchmark

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <iostream>
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
#include <utmpx.h>
#include <parallel/algorithm>

#include <omp.h>

#include "umap.h"
#include "testoptions.h"
#include "PerFile.h"

static const uint64_t IndexesSize = 20000000;
static uint64_t* Indexes;

// We initilize an array with a random set of indexes into our GIANT 600GB array
void initdata( uint64_t totalbytes )
{
  Indexes = new uint64_t [IndexesSize];

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint64_t> rnd_int(0, totalbytes-1);
#pragma omp parallel for
  for(uint64_t i = 0; i < IndexesSize; ++i)
    Indexes[i] = rnd_int(gen);
}

static inline uint64_t getns(void)
{
  struct timespec ts;
  int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  assert(ret == 0);
  return (((uint64_t)ts.tv_sec) * 1000000000ULL) + ts.tv_nsec;
}

int main(int argc, char **argv)
{
  umt_optstruct_t options;
  long pagesize;
  uint64_t totalbytes;
  void* base_addr;

  pagesize = umt_getpagesize();
  umt_getoptions(&options, argc, argv);
  omp_set_num_threads(options.numthreads);

  totalbytes = options.numpages*pagesize;
  base_addr = PerFile_openandmap(&options, totalbytes);
  assert(base_addr != NULL);
 
  fprintf(stdout, "%lu GB %lu pages, %lu threads\n", totalbytes/1024/1024/1024, options.numpages, options.numthreads);

  char *arr = (char *) base_addr; 

  uint64_t start = getns();
  initdata(totalbytes);
  fprintf(stdout, "Init took %f us\n", (double)(getns() - start)/1000000.0);

  start = getns();
#pragma omp parallel for
  for(uint64_t i = 0; i < IndexesSize; ++i)
    arr[Indexes[i]] += 1;

  uint64_t end = getns();
  fprintf(stdout, "%lu updates took %f seconds, %f updates per second\n", 
      IndexesSize,
      (double)(end - start)/100000000.0,
      (double)IndexesSize / (double)((double)(end - start)/100000000.0)
      );

  PerFile_closeandunmap(&options, totalbytes, base_addr);

  return 0;
}
