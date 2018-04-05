/*
 * This file is based on the flipx.c file of the ESO QFITS
 * Library.
 *
 * $Id: flipx.c,v 1.10 2006/02/17 10:26:58 yjung Exp $
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
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <cassert>
#include <string>
#include <omp.h>

#include "testoptions.h"
#include "PerFits.h"

#if 0
double torben(float **m, int n, int pos)
{
  int i, j, less, greater, equal;
  double min, max, guess, maxltguess, mingtguess;
  float num;

  hf.swapbyte(m[0]+pos, &num);
  min = max = num;

  for ( i = 1 ; i < n ; i++ ) {
    hf.swapbyte(m[i]+pos,&num);
    if (num<min) min=num;
    if (num>max) max=num;
  }

  while (1) {
    guess = (min+max)/2;
    less = 0; greater = 0; equal = 0;
    maxltguess = min ;
    mingtguess = max ;

    for ( j = 0; j < n; j++ ) {
      float m_swaped;
      hf.swapbyte(m[j] + pos, &m_swaped);
      if ( hf.fequal((double)m_swaped,guess) ) {
        equal++;
      }
      else if ( m_swaped < guess ) {
        less++;
        if ( m_swaped > maxltguess )
          maxltguess = m_swaped;
      } else {
        greater++;
        if ( m_swaped < mingtguess )
          mingtguess = m_swaped;
      }
    }

    if ( less <= (n + 1)/2 && greater <= (n + 1)/2 )
      break ;
    else if ( less > greater )
      max = maxltguess ;
    else
      min = mingtguess;
  }

  int half = (n + 1)/2;
  if ( less >= half )
    min=maxltguess;
  else
    min = mingtguess;

  if ( n & 1 )
    return min;
  if ( greater >= half )
    max = mingtguess;
  else if ( greater+equal >= half )
    max = guess;
  else
    max = maxltguess;
  return (min+max)/(double)2;
}

void median_calc(int n, struct patch *list, double *cube_median, float **d)
{
  uint64_t lx=list[0].ex;
  for ( int k = 1; k <= n; k++ ) {
#pragma omp parallel for
    for ( uint64_t i = list[k].sy; i < list[k].ey; i++ ) { // bounding box
      for ( uint64_t j = list[k].sx; j < list[k].ex; j++ ) {
        cube_median[i * lx + j] = torben((float **)d, hf.options.num_files, i * lx +j);
      }
    }
  }
}

static int process(const char * filename)
{
  int* fdlist;
  float* d[100];
  int lx, ly;
  int bpp;
  int dstart;
  off_t filesize;

  fdlist = (int *)calloc(hf.options.num_files, sizeof(*fdlist));

  for ( int i = 0; i < hf.options.num_files; i++ ) {
    struct stat fileinfo;
    std::string nfilename = filename + std::to_string(i+1) + ".fits";

    if ( hf.get_fits_image_info(nfilename) )
      continue;

    if ( stat(nfilename.c_str(), &fileinfo) ) {
      perror("stat failed: ");
      continue;
    }

    // FITS uses negative numbers to denote type and FLOAT is negative
    // But we only care about the size (at this point and not the type)
    hf.bitpix = abs(hf.bitpix);

    if ( i == 0 ) {
      lx = hf.naxes[0];
      ly = hf.naxes[1];
      bpp = hf.bitpix;
      dstart = hf.datastart;
      filesize = fileinfo.st_size;
    }

    assert("X Axes Length not uniform" && lx == hf.naxes[0]);
    assert("Y Axes Length not uniform" && ly == hf.naxes[0]);
    assert("Bits per pixel not uniform" && bpp == hf.bitpix);
    assert("Data offset (start) not uniform" && dstart == hf.datastart);
    assert("File sizes not uniform" && filesize == fileinfo.st_size);

    std::cout << nfilename 
      << ": size=" << hf.dataend-hf.datastart 
      << ", datastart=" << hf.datastart 
      << ", bitpix=" << hf.bitpix
      << ", naxis=" << hf.naxis
      << ", naxis1=" << hf.naxes[0]
      << ", naxis2=" << hf.naxes[1]
      << std::endl;

    fdlist[i] = open(nfilename.c_str(), O_RDWR);

    if ( fdlist[i] == -1 ) {
      perror("open");
      exit(-1);
    }
  }

  int psize = bpp / 8;

  omp_set_num_threads(hf.options.numthreads);
  int frame = dstart + lx * ly * psize;

  char* f1[100];
  for ( int i = 0; i < hf.options.num_files; i++ ) {
    f1[i] = (char*)mmap(0, filesize, PROT_READ|PROT_WRITE, MAP_SHARED, fdlist[i], 0);
    if ( f1[i] == (char*)-1 ) {
      perror("mmap");
      return -1 ;
    }
    d[i] = (float *)(f1[i] + dstart);
  }

  struct patch *list;
  int nlist, i;
  nlist = 0;
  std::ifstream input ("input.txt");
  if ( input.is_open() ) {
    input>>nlist;
    list = (struct patch *)calloc(nlist + 1, sizeof(struct patch));
    list[0].sx=0;
    list[0].sy=0;
    list[0].ex=lx;
    list[0].ey=ly;//boundry of the image
    i=0;
    while ( !input.eof() ) {
      i++;
      input>>list[i].sx>>list[i].ex>>list[i].sy>>list[i].ey;
    }
    input.close();
  }

  double *cube_median=(double *)malloc(sizeof(double)*lx*ly);
  double start = hf.gets();

  median_calc(nlist, list, cube_median, d);
  free(cube_median);
  free(list);
  for ( i = 0; i < hf.options.num_files; i++ ) {
    if ( munmap(f1[i], frame) != 0 ) {
      perror("unmapping file");
      return -1 ;
    }
  }
  return 0;
}
#endif

int main(int argc, char * argv[])
{
#if 0
  int err = 0;

  umt_getoptions(&hf.options, argc, argv);
  err += process(hf.options.filename);
  if ( err > 0 ) {
    std::cerr << argv[0] << ": " << err << " error(s) occurred\n";
    return -1 ;
  }
#endif
  return 0 ;
}
