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

extern "C"{
#include "../uffd_handler/uffd_handler.h"

volatile int stop_uffd_handler;
}

static inline uint64_t getns(void)
{
  struct timespec ts;
  int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  assert(ret == 0);
  return (((uint64_t)ts.tv_sec) * 1000000000ULL) + ts.tv_nsec;
}

typedef struct {
  int numpages;
  int numthreads;
  int bufsize;
  char *fn;
} optstruct_t;

optstruct_t options;

int main(int argc, char **argv)
{
  long num_pages;
  int uffd;
  long pagesize;
  int64_t totalbytes;
  pthread_t uffd_thread;
  int64_t arraysize;
  void *base_mmap_array;
  int value=0;
  pagesize = get_pagesize();

  options.numpages = NUMPAGES;
  options.numthreads = NUMTHREADS;
  options.bufsize= BUFFERSIZE;
  options.fn = NULL;
  num_pages= options.numpages;

  base_mmap_array = mmap(NULL, options.numpages*pagesize, PROT_READ|PROT_WRITE,
                MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
  if (base_mmap_array == MAP_FAILED)
  {
      perror("mmap");
      exit(1);
  }

  uint64_t*   array = (uint64_t*)  base_mmap_array; // feed it the mmaped region
  uint64_t    array_length = num_pages * 512;   // in number of 8-byte integers.
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

  //getoptions(&options, argc, argv);

  //totalbytes = options.numpages*pagesize;
  //openandmap(options.fn, totalbytes, p->fd,  p->base_addr);

  // start the thread that will handle userfaultfd events

  stop_uffd_handler = 0;

  params_t *p = (params_t *)malloc(sizeof(params_t));
  p->base_addr = (void *)array;
  p->pagesize = pagesize;
  p->bufsize = options.bufsize;
  p->faultnum = 0;
  p->uffd = uffd_init(p->base_addr, pagesize, num_pages);

  //fprintf(stdout, "%d pages, %d threads\n", options.numpages, options.numthreads);

  pthread_create(&uffd_thread, NULL, uffd_handler, p);

  sleep(1);

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
  stop_uffd_handler = 1;
  pthread_join(uffd_thread, NULL);
  //fprintf(stdout, "mode %llu\n", (unsigned long long)uffdio_register.mode);
  uffd_finalize(p,  options.numpages);

  for (long i=0;i<num_batches;i++)
  {
      fprintf(stdout,"%llu\n",(unsigned long long)latencies[i]);
  }

  free(latencies);
  munmap(base_mmap_array, pagesize * num_pages);
  return 0;
}
