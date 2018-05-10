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

#include <omp.h>
#include <fstream>

#include "testoptions.h"
#include "helper.hpp"
#include "PerFits.h"

helper_funs hf;

double torben(float *m, int n, uint64_t step)
{
  int i, less, greater, equal;
  double min, max, guess, maxltguess, mingtguess;
  float num;
  uint64_t j, maxj = n * step;

  hf.swapbyte(m, &num);
  min = max = num;
  j = (uint64_t)step;

  for ( i = 1 ; i < n ; i++ ) {
    hf.swapbyte(m + j, &num);

    if ( num < min )
      min = num;
    if ( num > max)
      max = num;
    j+=step;
  }

  while ( 1 ) {
    guess = (min + max) / 2;
    less = 0; greater = 0; equal = 0;
    maxltguess = min;
    mingtguess = max;

    for ( j = 0; j < maxj; j += step ) {
      float m_swaped;
      hf.swapbyte(m + j, &m_swaped);

      if ( m_swaped < guess ) {
        less++;
        if ( m_swaped > maxltguess )
          maxltguess = m_swaped;
      } else if ( m_swaped > guess ) {
        greater++;
        if ( m_swaped < mingtguess )
          mingtguess = m_swaped;
      } else
        equal++;
    }

    if ( less <= (n + 1) / 2 && greater <= (n + 1) / 2 )
      break;
    else if ( less > greater )
      max = maxltguess ;
    else
      min = mingtguess;
  }

  int half = (n + 1) / 2;
  min = ( less >= half ) ? maxltguess : mingtguess;

  if ( n & 1 )
    return min;

  if ( greater >= half )
    max = mingtguess;
  else if ( greater+equal >= half )
    max = guess;
  else
    max = maxltguess;
  return (min + max) / (double)2;
}

void median_calc(int n, struct patch *list, off_t zDim, double *cube_median, float *cube)
{
  uint64_t lx=list[0].ex;
  uint64_t ly=list[0].ey;
  for ( int k = 1; k <= n; k++ ) {
#pragma omp parallel for
    for ( uint64_t i = list[k].sy; i < list[k].ey; i++ ) { // bounding box
      for ( uint64_t j = list[k].sx; j < list[k].ex; j++ )
        cube_median[ i * lx + j ] = torben( cube + i * lx + j, zDim, lx*ly);
    }
  }
}  

static int run_test( void )
{
  float* mycube;
  size_t BytesPerElement;
  size_t xDim;
  size_t yDim;
  size_t zDim;

  mycube = (float*)PerFits::PerFits_alloc_cube(&hf.options, &BytesPerElement, &xDim, &yDim, &zDim);
  omp_set_num_threads(hf.options.numthreads);

  //median calculation
  double *cube_median = (double *)malloc(sizeof(double) * xDim * yDim);

  struct patch *list;
  int nlist, i;
  nlist=0;
  std::ifstream input ("input.txt");
  if ( input.is_open() ) {
    input >> nlist;
    list=(struct patch *)calloc(nlist+1,sizeof(struct patch));
    list[0].sx=0;
    list[0].sy=0;
    list[0].ex=xDim;
    list[0].ey=yDim;//boundry of the image
    i=0;
    while ( !input.eof() ) {
      i++;
      input >> list[i].sx >> list[i].ex >> list[i].sy >> list[i].ey;
    }
    input.close();
  }
  else {
    std::cerr << "Unable to find input.txt file\n";
    return -1;
  }

  double start = hf.gets();
  median_calc(nlist, list, zDim, cube_median, mycube);
  std::cout << "Median Calculation " << (double)(hf.gets() - start) << " s\n";

  //uf.displaycube(cube_median,list,nlist);
 
  free(list);
  free(cube_median);
  PerFits::PerFits_free_cube(mycube);
  return 0 ;
}

int main(int argc, char * argv[])
{
  umt_getoptions(&hf.options, argc, argv);

  run_test();

  return 0;
}
