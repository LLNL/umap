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
#include <random>
#include <iostream>

#define NUMPAGES 10000
#define NUMTHREADS 1
#define BUFFERSIZE 100

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

int main(int argc, char **argv)
{
  umt_optstruct_t options;
  long pagesize;
  int64_t totalbytes;
  void *base_addr;
  int value=0;
  int fd;

  pagesize = umt_getpagesize();

  umt_getoptions(&options, argc, argv);

  totalbytes = options.numpages*pagesize;

  fd = umt_openandmap(&options, totalbytes, &base_addr);

  uint64_t*   array = (uint64_t*)  base_addr; // feed it the mmaped region
  uint64_t    array_length = totalbytes/sizeof(int64_t);   // in number of 8-byte integers.
  uint64_t    experiment_count = 100000;   // Size of experiment, number of accesses
  uint64_t    batch_size = 1000;  // Set a batch size MUST BE MULTIPLE OF experiment_count
  std::vector<uint64_t>  vec_random_indices;
  vec_random_indices.reserve(experiment_count);

  std::random_device rd;  //Will be used to obtain a seed for the random number engine
  std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<> dis(0, array_length);
  for(uint64_t i=0; i<experiment_count; ++i)
  {
      vec_random_indices.push_back(dis(gen));
  }

  int num_batches=vec_random_indices.size()/batch_size;

  uint64_t *latencies =(uint64_t *) malloc(sizeof(uint64_t)*num_batches);
  assert(latencies);
  memset(latencies,0,sizeof(uint64_t)*num_batches);

  //fprintf(stdout, "%d pages, %d threads\n", options.numpages, options.numthreads);

  omp_set_num_threads(options.numthreads);

  //  Fetch indices in batches
  //
  for(uint64_t i=0; i<vec_random_indices.size(); i += batch_size)
  {
      // START TIMER
      uint64_t dummy_value(0);
      //printf("%d\n",i);
      uint64_t start = getns();
#pragma omp parallel for reduction(+:value)
      for(uint64_t j=0; j<batch_size;++j)
      {
          //printf("i,j vec %d %d %d\n",i,j,vec_random_indices[i +j]);
          //dummy_value += array[0];
          //dummy_value += vec_random_indices[i +j];
          dummy_value += array[vec_random_indices[i +j]];
        //printf("i,j %d %d\n",i,j);
      }
      uint64_t dur = getns()-start;
      latencies[i/batch_size]=dur/batch_size;
      //printf("->");
      // END TIMER
      // CALC LATENCY & IOPS
  }

  printf("\n");
  umt_closeandunmap(&options, totalbytes, base_addr, fd);

  for (long i=0;i<num_batches;i++)
  {
      fprintf(stdout,"%llu\n",(unsigned long long)latencies[i]);
  }

  free(latencies);
  return 0;
}
