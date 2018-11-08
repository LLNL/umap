/*
 * This file is part of UMAP.  For copyright information see the COPYRIGHT file
 * in the top level directory, or at
 * https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free
 * software; you can redistribute it and/or modify it under the terms of the
 * GNU Lesser General Public License (as published by the Free Software
 * Foundation) version 2.1 dated February 1999.  This program is distributed in
 * the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the terms and conditions of the GNU Lesser General Public License for more
 * details.  You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
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

#ifdef _OPENMP
#include <omp.h>
#endif

#include "umap/umap.h"
#include "../utility/commandline.hpp"
#include "../utility/umap_file.hpp"

#define handle_error_en(en, msg) \
  do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

void cpu_setcpu(int cpu)
{
  int s;
  cpu_set_t cpuset;
  pthread_t thread;

  thread = pthread_self();

  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);

  s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (s != 0)
    handle_error_en(s, "pthread_setaffinity_np");

  /* Check the actual affinity mask assigned to the thread */

  s = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (s != 0)
    handle_error_en(s, "pthread_getaffinity_np");
}
static inline uint64_t getns(void)
{
  struct timespec ts;
  int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  assert(ret == 0);
  return (((uint64_t)ts.tv_sec) * 1000000000ULL) + ts.tv_nsec;
}

void initdata(uint64_t *region, int64_t rlen) {
  fprintf(stdout, "initdata: %p, %ld\n", region, rlen);
#pragma omp parallel for
  for(int64_t i=0; i< rlen; ++i) {
    region[i] = (uint64_t) (rlen - i);
  }
}

int main(int argc, char **argv)
{
  utility::umt_optstruct_t options;
  long pagesize;
  int64_t totalbytes;
  uint64_t arraysize;
  void* base_addr;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> rnd_int(0, 39);

  pagesize = utility::umt_getpagesize();

  umt_getoptions(&options, argc, argv);

  omp_set_num_threads(options.numthreads);

  totalbytes = options.numpages*pagesize;
  base_addr = utility::map_in_file(options.filename, options.initonly,
      options.noinit, options.usemmap, totalbytes);
  assert(base_addr != NULL);
 
  fprintf(stdout, "%lu pages, %lu threads\n", options.numpages, options.numthreads);

  uint64_t *arr = (uint64_t *) base_addr; 
  arraysize = totalbytes/sizeof(int64_t);

  uint64_t start = getns();
  if ( !options.noinit ) {
    // init data
    initdata(arr, arraysize);
    fprintf(stdout, "Init took %f us\n", (double)(getns() - start)/1000000.0);
  }

  const int testpages = 400;

  if ( !options.initonly ) 
  {
    std::vector<int> cpus{0, 10, 20, 30};

    start = getns();
#pragma omp parallel for
    for (uint64_t page = 0; page < options.numpages - testpages; page += testpages) {
      uint64_t sum = 0;

      //cpu_setcpu(10);
      for (int x = 0; x < testpages; x++) {
        uint64_t* p = &arr[(page+x)*(pagesize/sizeof(uint64_t*))];
        sum += *p;
      }

      cpu_setcpu(rnd_int(gen));

      //cpu_setcpu(30);
      for (int x = 0; x < testpages; ++x) {
        uint64_t* p = &arr[(page+x)*(pagesize/sizeof(uint64_t*))];
        *p = sum;
      }
    }

    fprintf(stdout, "test took %f us\n", (double)(getns() - start)/1000000.0);
  }
  
  utility::unmap_file(options.usemmap, totalbytes, base_addr);

  return 0;
}
