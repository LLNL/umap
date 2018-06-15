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

#include "median_calculation_kernel.hpp"
#include "testoptions.h"
#include "PerFits.h"

const float x_intercept[6] = {1058.2, 1325.606, 1010.564, 829.674, 1390.826, 1091.015};
const float x_slope[6] = {3.5, 3.5, 3.5, 3.5, 3.5, 3.5};
const float y_intercept[6] = {124, 424, 724, 1024, 1324, 1624};
const float y_slope[6] = {0, 0, 0, 0, 0, 0};

int main(int argc, char** argv)
{
  umt_optstruct_t options;
  umt_getoptions(&options, argc, argv);

  size_t BytesPerElement;
  size_t xDim;
  size_t yDim;
  size_t zDim;
  //float *cube_pointer = new float[4096 * 4096 * 10]; // just a dummy memory region for test
  median::cube_t<float> cube;

  cube.data = (float*)PerFits::PerFits_alloc_cube(&options, &BytesPerElement, &xDim, &yDim, &zDim);

  cube.size_x = xDim;
  cube.size_y = yDim;
  cube.size_k = zDim; // tne number of frames

  for (int i = 0; i < 6; ++i) {
    std::cout << "\nEpoch: " << i << std::endl;
    const auto ret = median::torben(cube, {x_intercept[i], x_slope[i], y_intercept[i], y_slope[i]});
    std::cout << "median value: " << ret.first << std::endl;
  }

  PerFits::PerFits_free_cube(cube.data);
}
