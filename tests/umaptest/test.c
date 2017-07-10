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
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>    // optind
#include <errno.h>
#include <time.h>

#define NUMPAGES 10000000
#define NUMTHREADS 2
#define BUFFERSIZE 16

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
  long pagesize;
  long num_pages;
  void *region;
  umt_optstruct_t options;
  int fd;

  pagesize = umt_getpagesize();

  umt_getoptions(&options, argc, argv);
  
  num_pages = options.numpages;
  omp_set_num_threads(options.numthreads);
  
  fd = umt_openandmap(&options, options.numpages*pagesize, &region);

  fprintf(stdout, "%ld pages, %lu threads\n", num_pages, options.numthreads);
  fprintf(stdout, "USEFILE enabled %s\n", options.fn);

  // storage for the latencies for each page
  int num_batches = 10;
  uint64_t *latencies = malloc(sizeof(uint64_t) * num_batches);
  assert(latencies);
  memset(latencies, 0, sizeof(uint64_t) * num_batches);

  // measure latency in batches
  long batch_size=num_pages/num_batches;  

  // touch each page in the region
  int value=0;
  int *cur = region;

#pragma omp parallel for reduction(+:value) //private (value)
  for (long i = 0; i < num_pages; i+=batch_size) {
    uint64_t start = getns();
    for (long j=0;j<batch_size&& (i+j<num_pages);j++)
      {
	//fprintf(stdout, "mode %llu\n", (unsigned long long)uffdio_register.mode);
	//fprintf(stdout, "cur %x, adddress at i:%d, j:%d, %x\n", cur, i, j, &cur[i*1024 + j*1024]);
	int v = cur[i*1024 + j*1024 + 5];
	cur[i*1024 + j*1024 + 5] = i+j;
	//int v = *cur;
	//fprintf(stdout, "%llu\n", (unsigned long long)latencies[i]);
	//fprintf(stdout, "mode %llu\n", (unsigned long long)uffdio_register.mode);
	//fprintf(stdout,"%d\n",faultnum);
	value += v;
	  
#if 0
	int ret = madvise(cur+(i*1024 + j*1024), pagesize, MADV_DONTNEED);
	if(ret == -1) { perror("madvise"); assert(0); } 
#endif

      }
    uint64_t dur = getns() - start;
    latencies[i/batch_size] = dur/batch_size;
    //fprintf(stdout, "%llu\n", (unsigned long long)latencies[i]);
    fprintf(stdout, "."); fflush(stdout);
  }
  fprintf(stdout,"\n");
  //------------------check when page is loaded----------------------

  /* #pragma omp parallel for private (value) */
  /*     for (long i = 0; i < num_pages; i+=1) { */
  /*         uint64_t start = getns(); */
  /*         //fprintf(stdout, "mode %llu\n", (unsigned long long)uffdio_register.mode); */
  /* 	//        int v = *((int*)cur); */
  /*         int v = cur[i*1024]; */
  /*         uint64_t dur = getns() - start; */
  /*         latencies[i] = dur; */
  /*         //fprintf(stdout, "%llu\n", (unsigned long long)latencies[i]); */
  /*         //fprintf(stdout, "mode %llu\n", (unsigned long long)uffdio_register.mode); */
  /*         //fprintf(stdout,"%d\n",faultnum); */
  /*         value += v; */
  /* 	int ret = madvise(&cur[i*1024], pagesize, MADV_DONTNEED); */
  /* 	if(ret == -1) { perror("madvise"); assert(0); } */
  /*         //cur += pagesize; */
  /*     } */
  //-------------------------------------------------------------------

  umt_closeandunmap(&options, options.numpages*pagesize, region, fd);

  for (long i = 0; i < num_batches; i++) {
    fprintf(stdout, "%llu\n", (unsigned long long)latencies[i]);
  }

  free(latencies);

  return 0;
}
