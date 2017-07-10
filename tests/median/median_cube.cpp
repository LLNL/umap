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

#define NUMPAGES 10000000
#define NUMTHREADS 2
#define BUFFERSIZE 16

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

uint64_t *cube,*cube_median;
int size_a,size_b,size_c;

void initdata(uint64_t *region, int64_t rlen) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint64_t> rnd_int;
#pragma omp parallel for
  for(int i=0; i< rlen; ++i) {
    region[i] = (uint64_t) (rlen - i);// rnd_int(gen);
    //region[i] = rnd_int(gen)>>1;//divide all values by 2 because of overflow in torben
    //printf("%lld\n", (long long)region[i]);
  }
}
uint64_t torben(uint64_t *m, int n,int step)
{
  int         i,j, less, greater, equal;
  uint64_t  min, max, guess, maxltguess, mingtguess;

  min = max = m[0] ;
  j=step;
  for (i=1 ; i<n ; i++) {
    if (m[j]<min) min=m[j];
    if (m[j]>max) max=m[j];
    j+=step;
    //fprintf(stdout,"m:%llu\n",m[i]);
  }
  //fprintf(stdout,"Max:%llu\nMin:%llu\n",max,min);

  while (1) {
    guess = (min+max)/2;
    less = 0; greater = 0; equal = 0;
    maxltguess = min ;
    mingtguess = max ;
#pragma omp parallel for reduction(+:less,greater,equal),reduction(max:maxltguess),reduction(min:mingtguess)
    for (j=0; j<n*step;j+=step)
      {
	if (m[j]<guess) {
	  less+=step;
	  if (m[j]>maxltguess) maxltguess = m[j] ;
	} else if (m[j]>guess) {
	  greater+=step;
	  if (m[j]<mingtguess) mingtguess = m[j] ;
	} else equal+=step;
      }

    if (less <= step*(n+1)/2 && greater <= step*(n+1)/2) break ;
    else if (less>greater) max = maxltguess ;
    else min = mingtguess;
    //fprintf(stdout,"guess: %llu less:%d greater:%d\n",guess,less,greater);
  }
  if (less >= step*(n+1)/2) return maxltguess;
  else if (less+equal >= step*(n+1)/2) return guess;
  else return mingtguess;
}
void getall_median()
{
  int i,j;
  cube_median=(uint64_t *)malloc(sizeof(uint64_t)*size_a*size_b);
  for (i=0;i<size_a;i++)
    for (j=0;j<size_b;j++)
      cube_median[i*size_b+j]=torben(cube+i*size_b+j,size_c,size_a*size_b);
}
void displaycube(uint64_t *cube,int a,int b,int c)
{
  int i,j,k;
  for (k=0;k<c;k++)
    {
      for (i=0;i<a;i++)
        {
	  for (j=0;j<b;j++)
	    printf("%lu ",cube[k*a*b+i*b+j]);
	  printf("\n");
        }
      printf("**************\n");
    }
}
int main(int argc, char **argv)
{
  umt_optstruct_t options;
  long pagesize;
  int64_t totalbytes;
  int64_t arraysize;
  uint64_t median;
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
  fprintf(stdout, "Array size: %ld\n", arraysize);

  uint64_t start = getns();
  size_a=10;
  size_b=10;
  size_c=9;
  // init data
  initdata(arr, size_a*size_b*size_c);
  cube=arr;
  displaycube(cube,size_a,size_b,size_c);
  fprintf(stdout, "Init took %f us\n", (double)(getns() - start)/1000000.0);

  start = getns();
  getall_median();
  //median=torben(arr,arraysize);
  fprintf(stdout, "Median is %llu, Find median took %f us\n",median,(double)(getns() - start)/1000000.0);
  int i,j;
  for (i=0;i<size_a;i++)
    {
      for (j=0;j<size_b;j++)
	printf("%d ",cube_median[i*size_a+j]);
      printf("\n");
    }
  free(cube_median);

  umt_closeandunmap(&options, totalbytes, base_addr, fd);

  return 0;
}
