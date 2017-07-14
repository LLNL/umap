/* $Id: flipx.c,v 1.10 2006/02/17 10:26:58 yjung Exp $
 *
 * This file is part of the ESO QFITS Library
 * Copyright (C) 2001-2004 European Southern Observatory
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * $Author: yjung $
 * $Date: 2006/02/17 10:26:58 $
 * $Revision: 1.10 $
 * $Name: qfits-6_2_0 $
 */

/*-----------------------------------------------------------------------------
                                   Includes
 -----------------------------------------------------------------------------*/
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include <string>

#define NUMPAGES 10000
#define NUMTHREADS 1
#define BUFFERSIZE 100

#include "umap.h"
#include "umaptest.h"

#ifdef _OPENMP
#include <omp.h>
#endif

extern "C"
{
#include "qfits.h"
}

umt_optstruct_t options;

static void copypix(char * buf,char * buf2, int p1, int psize)
{
    uint16_t a1,b1;
    uint32_t a2,b2;
    if (psize==2) 
    {
      a1=*(uint16_t *)(buf+p1);
      //b1=*(uint16_t *)(buf2+p1);
      printf("%u\n",a1);
      //if (a1!=b1) printf("%u %u\n",a1,b1);
    }
    else
    {
      a2=*(uint32_t *)(buf+p1);
      //b2=*(uint32_t *)(buf2+p1);
      printf("%lu\n",a2);
      //if (a2!=b2) printf("%lu %lu\n",a2,b2);
    }
}

uint64_t torben(uint32_t *m, int n,int step)
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
void displaycube(uint64_t *cube,int a,int b,int c)
{
  int i,j,k;
  for (k=0;k<c;k++)
    {
      for (i=0;i<a;i++)
        {
	  for (j=0;j<b;j++)
	    printf("%llu",cube[k*a*b+i*b+j]);
	  printf("\n");
        }
      printf("**************\n");
    }
}
static int test_openfiles(const char *fn)
{
    long pagesize;
    int64_t totalbytes;
    void *base_addr;
    off_t frame;
    char filename[100];
    int *fd_list;

    pagesize = umt_getpagesize();
    strcpy(filename,fn);
    strcat(filename,"1");
    strcat(filename,".fits");

    char        *    sval ;
    int                dstart;
    int                lx, ly ;
    int                bpp ;
    int                i, j ;
    char        *    buf ;
    int                psize;
    int segsize;

    /* Retrieve image attributes */
    if (qfits_is_fits(filename)!=1) {
        printf("not a FITS file\n");
        return -1 ;
    }

    sval = qfits_query_hdr(filename, "NAXIS1");
    if (sval==NULL) {
        printf("cannot read NAXIS1\n");
        return -1 ;
    }
    lx = atoi(sval);
    sval = qfits_query_hdr(filename, "NAXIS2");
    if (sval==NULL) {
        printf("cannot read NAXIS2\n");
        return -1 ;
    }
    ly = atoi(sval);
    sval = qfits_query_hdr(filename, "BITPIX");
    if (sval==NULL) {
        printf("cannot read BITPIX\n");
        return -1 ;
    }
    bpp = atoi(sval);

    psize = bpp/8 ;
    //printf("psize: %d uint32: %d\n",psize,sizeof(uint32_t));
    if (psize<0) psize=-psize ;

    /* Retrieve start of first data section */
    if (qfits_get_datinfo(filename, 0, &dstart,&segsize)!=0) {
        printf("reading header information\n");
        return -1 ;
    }

    printf("psize:%d\n",psize);
    printf("dstart:%d\n",dstart);
    frame=(off_t)lx*ly*psize;
    fd_list = umt_openandmap_fits(&options, options.numpages*pagesize,&base_addr,(off_t)dstart,frame);

    buf=(char *)base_addr;
    omp_set_num_threads(options.numthreads);
    printf("region start:%p\n",base_addr);

    printf("lx ly:%d %d\n",lx,ly);

    //median calculation
    uint64_t *cube_median=(uint64_t *)malloc(sizeof(uint64_t)*lx*ly);
    uint32_t *cube=(uint32_t *)base_addr; 
    for (i=0 ; i<ly ; i++)
    {
        for (j=0 ; j<lx ; j++)
	{
	  //copypix(buf+4*lx*ly*psize,buf+lx*ly*psize,j,psize);
	    cube_median[i*lx+j]=torben(cube+i*ly+j,options.fnum,lx*ly);
	}
	buf+=lx*psize;
    }
    
    displaycube(cube_median,ly,lx,1);
    umt_closeandunmap_fits(&options, totalbytes, base_addr, fd_list);
    return 0 ;
}

/*-----------------------------------------------------------------------------
                                Main
 -----------------------------------------------------------------------------*/
int main(int argc, char * argv[])
{
    int err ;
    umt_getoptions(&options, argc, argv);
    err=0;
    test_openfiles(options.fn);
    //err += fits();
    if (err>0)
    {
        fprintf(stderr, "%s: %d error(s) occurred\n", argv[0], err);
        return -1 ;
    }
    return 0 ;
}
