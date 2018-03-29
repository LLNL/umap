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
#include <math.h>
#include <assert.h>
#include <fstream>

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

struct patch
{
  int sx,sy,ex,ey;
}; // boundries for each patches

static inline double gets(void)
{
  return omp_get_wtime();
}
void swapbyte(float *a,float *b)
{
    char *a1=(char *)a;
    char *b1=(char *)b;
    b1[0]=a1[3];
    b1[3]=a1[0];
    b1[1]=a1[2];
    b1[2]=a1[1];
}
bool fequal(double a, double b)
{
  if (fabs(a-b)<(1e-6)) return 1;
  else return 0;
}
double torben(float **m, int n,int pos)
{
  int         i,j, less, greater, equal;
  double  min, max, guess, maxltguess, mingtguess;
  float num;

  swapbyte(m[0]+pos,&num);
  min = max = num;
  //fprintf(stdout,"m:%6.5lf\n",num);

  for (i=1 ; i<n ; i++) 
  {
      swapbyte(m[i]+pos,&num);
      if (num<min) min=num;
      if (num>max) max=num;
      //fprintf(stdout,"m:%6.5lf\n",num);
  }
  //fprintf(stdout,"Max:%6.5lf\nMin:%6.5lf\n",max,min);

  while (1) {
    guess = (min+max)/2;
    less = 0; greater = 0; equal = 0;
    maxltguess = min ;
    mingtguess = max ;
    for (j=0; j<n;j++)
      {
	float m_swaped;
	//fprintf(stdout,"j:%d\n",j);
	swapbyte(m[j]+pos,&m_swaped);
	if (fequal((double)m_swaped,guess))
	{
	    equal++;
	    //printf("%6.5lf, %6.5lf\n",m_swaped,guess);
        }
	else if (m_swaped<guess)
	{
	  less++;
	  if (m_swaped>maxltguess) maxltguess = m_swaped;
	} else {
	  greater++;
	  //printf("%6.5lf, %6.5lf\n",m_swaped,mingtguess);
	  if (m_swaped<mingtguess) mingtguess = m_swaped;
	}
      }
    if (less <= (n+1)/2 && greater <= (n+1)/2) break ;
    else if (less>greater) max = maxltguess ;
    else min = mingtguess;
  }
  //fprintf(stdout,"guess: %6.5lf less:%d greater:%d equal:%d all:%d\n",guess,less,greater,equal,(n+1)/2);
  int half=(n+1)/2;
  if (less>=half) min=maxltguess;
  else min=mingtguess;
  if (n&1) return min;
  if (greater >= half) max = mingtguess;
  else if (greater+equal >= half) max = guess;
  else max = maxltguess;
  return (min+max)/(double)2;
}
void displaycube(double *cube,struct patch *list,int n)
{
     int i,j,k;
     uint64_t lx=list[0].ex;
     //uint64_t ly=list[0].ey;
     for (k=1;k<=n;k++)
     {
	 for (i=list[k].sy; i<list[k].ey; i++) // bounding box
	 {
	     for (j=list[k].sx; j<list[k].ex; j++)
	     {
		 printf("%6.5lf\n",cube[i*lx+j]);		 
		 //printf("\n");
	     }
	 }
     }
}
void median_calc(int n,struct patch *list,double *cube_median,float **d)
{
    int lx=list[0].ex;
    //int ly=list[0].ey;
    for (int k=1;k<=n;k++)
    {
	#pragma omp parallel for
        for (int i=list[k].sy; i<list[k].ey; i++) // bounding box
	{
            for (int j=list[k].sx; j<list[k].ex; j++)
	    {
	        //printf("i,j:%d %d\n",i,j);
 	        cube_median[i*lx+j]=torben(d,options.fnum,i*lx+j);
	    }
        }
   }
}  
static int test_openfiles(const char *fn)
{
    long pagesize;
    int64_t totalbytes;
    void *base_addr[100];
    off_t frame;
    char filename[100];
    void *bk_list;

    pagesize = umt_getpagesize();
    strcpy(filename,fn);
    strcat(filename,"1");
    strcat(filename,".fits");

    puts(filename);
    char        *    sval ;
    int                dstart;
    int                lx, ly ;
    int                bpp ;
    int                i;
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

    //printf("psize:%d\n",psize);
    //printf("dstart:%d\n",dstart);
    frame=(off_t)lx*ly*psize;
    //printf("psize:%d lx:%d ly:%d\n",frame,lx,ly);
    totalbytes=options.numpages*pagesize;
    bk_list = umt_openandmap_fits2(&options,totalbytes,base_addr,(off_t)dstart,frame);
    assert(bk_list != NULL);

    printf("thread num:%lu\n",options.numthreads);
    omp_set_num_threads(options.numthreads);
    printf("region start:%p\n",base_addr);

    //printf("lx ly:%d %d\n",lx,ly);

    //median calculation
    double *cube_median=(double *)malloc(sizeof(double)*lx*ly);
    float *cube[100];
    for (i=0;i<options.fnum;i++)
	cube[i]=(float *)((uint8_t *)base_addr[i]+dstart);

    struct patch *list;
    int nlist;
    nlist=0;
    std::ifstream input ("input.txt");
    if (input.is_open())
    {
      input>>nlist;
      list=(struct patch *)calloc(nlist+1,sizeof(struct patch));
      list[0].sx=0;
      list[0].sy=0;
      list[0].ex=lx;
      list[0].ey=ly;//boundry of the image
      i=0;
      while (i<nlist)
      {
          i++;
          input>>list[i].sx>>list[i].ex>>list[i].sy>>list[i].ey;
      }
      input.close();
    }

    double start = gets();
    median_calc(nlist,list,cube_median,cube);
    fprintf(stdout, "Median Calculation %f s\n", (double)(gets() - start));
    //displaycube(cube_median,list,nlist);
    umt_closeandunmap_fits2(&options, totalbytes, base_addr, bk_list);
    free(cube_median);
    free(list);
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
