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


#define NUMPAGES 10000000
#define NUMTHREADS 2
#define BUFFERSIZE 16

#include "umap.h"
#include "umaptest.h"

#ifdef _OPENMP
#include <omp.h>
#endif

static inline uint64_t getns(void)
{
  struct timespec ts;
  int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  assert(ret == 0);
  return (((uint64_t)ts.tv_sec) * 1000000000ULL) + ts.tv_nsec;
}

void initdata(uint64_t *region, int64_t rlen) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint64_t> rnd_int;
#pragma omp parallel for
  for(int64_t i=0; i< rlen; ++i) {
    region[i] = (uint64_t) (rlen - i);// rnd_int(gen);
    //region[i] = rnd_int(gen)>>1;//divide all values by 2 because of overflow in torben
    //printf("%llu\n", (long long)region[i]);
  }
}
uint64_t torben(uint64_t *m, int n)
{
    int         i, less, greater, equal;
    uint64_t  min, max, guess, maxltguess, mingtguess;

    min = max = m[0] ;
    for (i=1 ; i<n ; i++) {
        if (m[i]<min) min=m[i];
        if (m[i]>max) max=m[i];
        //if (m[i]>n) fprintf(stdout,"m:%llu\n",m[i]);
    }
    //fprintf(stdout,"Max:%llu\nMin:%llu\n",max,min);

    while (1) {
        guess = (min+max)/2;
        less = 0; greater = 0; equal = 0;
        maxltguess = min ;
        mingtguess = max ;
#pragma omp parallel for reduction(+:less,greater,equal),reduction(max:maxltguess),reduction(min:mingtguess)
        for (i=0; i<n; i++) {
            if (m[i]<guess) {
                less++;
                if (m[i]>maxltguess) maxltguess = m[i] ;
            } else if (m[i]>guess) {
                greater++;
                if (m[i]<mingtguess) mingtguess = m[i] ;
            } else equal++;
        }

        if (less <= (n+1)/2 && greater <= (n+1)/2) break ;
        else if (less>greater) max = maxltguess ;
        else min = mingtguess;
        //fprintf(stdout,"guess: %llu less:%d greater:%d\n",guess,less,greater);
    }
    if (less >= (n+1)/2) return maxltguess;
    else if (less+equal >= (n+1)/2) return guess;
    else return mingtguess;
}
int main(int argc, char **argv)
{
  umt_optstruct_t options;
  long pagesize;
  int64_t totalbytes;
  int64_t arraysize;
  uint64_t median;
  void* base_addr;
  void* maphandle;

  pagesize = umt_getpagesize();

  umt_getoptions(&options, argc, argv);

  totalbytes = options.numpages*pagesize;
  maphandle = umt_openandmap(&options, totalbytes, &base_addr);
  assert(maphandle != NULL);

  fprintf(stdout, "%lu pages, %lu threads\n", options.numpages, options.numthreads);

  omp_set_num_threads(options.numthreads);

  uint64_t *arr = (uint64_t *) base_addr;
  arraysize = totalbytes/sizeof(int64_t);
  fprintf(stdout,"Array size: %ld\n",arraysize);

  uint64_t start = getns();
  // init data
  initdata(arr, arraysize);
  fprintf(stdout, "Init took %f us\n", (double)(getns() - start)/1000000.0);

  start = getns();
  median=torben(arr,arraysize);
  fprintf(stdout, "Median is %lu, Find median took %f us\n",median,(double)(getns() - start)/1000000.0);

  umt_closeandunmap(&options, totalbytes, base_addr, maphandle);
  return 0;
}

