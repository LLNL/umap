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
#include <math.h>
#include <fstream>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "umaptest.h"

extern "C"
{
#include "qfits.h"
}

umt_optstruct_t options;
struct patch
{
  uint64_t sx,sy,ex,ey;
}; // boundries for each patches


static inline double gets(void)
{
  return omp_get_wtime();
}

bool fequal(double a, double b)
{
  if (fabs(a-b)<(1e-6)) return 1;
  else return 0;
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
     //unsigned int i,j,k;
     uint64_t lx=list[0].ex;
     //uint64_t ly=list[0].ey;
     for (int k=1;k<=n;k++)
     {
	 for (unsigned int i=list[k].sy; i<list[k].ey; i++) // bounding box
	 {
	     for (unsigned int j=list[k].sx; j<list[k].ex; j++)
	     {
		 printf("%6.5lf\n",cube[i*lx+j]);		 
		 //printf("\n");
	     }
	 }
     }
}

void median_calc(int n,struct patch *list,double *cube_median,float **d)
{
    uint64_t lx=list[0].ex;
    //uint64_t ly=list[0].ey;
    for (int k=1;k<=n;k++)
    {
	#pragma omp parallel for
        for (uint64_t i=list[k].sy; i<list[k].ey; i++) // bounding box
	{
            for (uint64_t j=list[k].sx; j<list[k].ex; j++)
	    {
 	        cube_median[i*lx+j]=torben((float **)d,options.num_files,i*lx+j);
	    }
        }
   }
}

static int process(const char * filename)
{
    char *nfilename;
    int *fdlist;
    char num[5];
    float *d[100];
    fdlist=(int *)calloc(500,sizeof(int));
    nfilename=(char *)malloc(sizeof(char)*100);

    for (int i=0;i<options.num_files;i++)
    {
        strcpy(nfilename,filename);
	sprintf(num,"%d",i+1);
	strcat(nfilename,num);
	strcat(nfilename,".fits");
	//printf("processing %s\n",nfilename);
	fdlist[i] = open(nfilename, O_RDWR);

	if(fdlist[i] == -1) 
	{
	    perror("open");
	    exit(-1);
	}
    }
    char        *    sval ;
    int                dstart;
    int                lx, ly ;
    int                bpp ;
    int                psize;
    struct stat        fileinfo ;
    int segsize;

    //printf("processing %s\n",filename);

    if (stat(nfilename, &fileinfo)!=0) {
        return -1 ;
    }
    if (fileinfo.st_size<1) {
        printf("cannot stat file\n");
        return -1 ;
    }

    /* Retrieve image attributes */
    if (qfits_is_fits(nfilename)!=1) {
        printf("not a FITS file\n");
        return -1 ;
    }

    sval = qfits_query_hdr(nfilename, "NAXIS1");
    if (sval==NULL) {
        printf("cannot read NAXIS1\n");
        return -1 ;
    }
    lx = atoi(sval);
    sval = qfits_query_hdr(nfilename, "NAXIS2");
    if (sval==NULL) {
        printf("cannot read NAXIS2\n");
        return -1 ;
    }
    ly = atoi(sval);
    sval = qfits_query_hdr(nfilename, "BITPIX");
    if (sval==NULL) {
        printf("cannot read BITPIX\n");
        return -1 ;
    }
    bpp = atoi(sval);

    psize = bpp/8 ;
    //printf("psize: %d uint32: %d\n",psize,sizeof(uint32_t));
    if (psize<0) psize=-psize ;

    /* Retrieve start of first data section */
    if (qfits_get_datinfo(nfilename, 0, &dstart, &segsize)!=0) {
        printf("reading header information\n");
        return -1 ;
    }

    omp_set_num_threads(options.numthreads);
    //printf("dstart:%d\n",dstart);
    // printf("psize:%d\n",psize);
    // printf("dstart:%d\n",dstart);
    // printf("segsize:%d\n",segsize);
    //Map the input file in read/write mode (input file is modified)
    int frame=dstart+lx*ly*psize;
    //printf("frame size:%d\n",frame);
    char *f1[100];
    for (int i=0;i<options.num_files;i++)
    {
        f1[i] = (char*)mmap(0,fileinfo.st_size,PROT_READ | PROT_WRITE,MAP_SHARED,fdlist[i],0);
	if (f1[i]==(char*)-1) 
	{
	    perror("mmap");
            printf("mapping file\n");
            return -1 ;
        }
	d[i]=(float *)(f1[i]+dstart);
    }

    struct patch *list;
    int nlist,i;
    //double *data=(double *)fbuf+dstart;
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
      while (!input.eof())
      {
          i++;
          input>>list[i].sx>>list[i].ex>>list[i].sy>>list[i].ey;
      }
      input.close();
    }

    double *cube_median=(double *)malloc(sizeof(double)*lx*ly);
    double start = gets();
    median_calc(nlist,list,cube_median,d);
    fprintf(stdout, "Median Calculation %f s\n", (double)(gets() - start));
    //displaycube(cube_median,list,nlist);
    free(cube_median);
    free(list);
    for (i=0;i<options.num_files;i++)
    {
        if (munmap(f1[i], frame)!=0) 
        {
	    printf("unmapping file %d\n",i);
            return -1 ;
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------------
                                Main
 -----------------------------------------------------------------------------*/
int main(int argc, char * argv[])
{
    int err ;
    umt_getoptions(&options, argc, argv);
    err=0;
    err += process(options.filename);
    if (err>0)
    {
        fprintf(stderr, "%s: %d error(s) occurred\n", argv[0], err);
        return -1 ;
    }
    return 0 ;
}
