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

#include <iostream>
#include <cmath>

#include "torben.hpp"
#include "utility.hpp"
#include "vector.hpp"
#include "../utility/commandline.hpp"
#include "../utility/umap_fits_file.hpp"

using pixel_type = float;

constexpr size_t num_vectors = 6;
const pixel_type x_intercept[num_vectors] = {1058.2, 1325.606, 1010.564, 829.674, 1390.826, 1091.015};
const pixel_type x_slope[num_vectors] = {3.5, 3.5, 3.5, 3.5, 3.5, 3.5};
const pixel_type y_intercept[num_vectors] = {124, 424, 724, 1024, 1324, 1624};
const pixel_type y_slope[num_vectors] = {0, 0, 0, 0, 0, 0};
const pixel_type correct_median[num_vectors] = {14913.25, 15223.21, 2284.29, 8939.15, 24899.55, 2395.80};


int main(int argc, char** argv)
{
  utility::umt_optstruct_t options;
  umt_getoptions(&options, argc, argv);

  size_t BytesPerElement;
  median::cube_t<float> cube;

  cube.data = (float*)utility::umap_fits_file::PerFits_alloc_cube(
      options.filename, &BytesPerElement, &cube.size_x, &cube.size_y, &cube.size_k);

  for (int i = 0; i < num_vectors; ++i) {
    std::cout << "Vector " << i << std::endl;

    vector_t vector{x_intercept[i], x_slope[i], y_intercept[i], y_slope[i]};
    vector_iterator<pixel_type> begin(cube, vector, 0);
    vector_iterator<pixel_type> end(vector_iterator<pixel_type>::create_end(cube, vector));

    // median calculation w/ Torben algorithm
    const auto median_val = torben(begin, end);

    // Check the result
    std::cout.setf(std::ios::fixed, std::ios::floatfield);
    std::cout.precision(2);
    if (std::fabs(median_val - correct_median[i]) < 0.01) {
      std::cout << " Correct " <<  median_val << " == " << correct_median[i] << std::endl;
    } else {
      std::cerr << " Error " <<  median_val << " != " << correct_median[i] << std::endl;
      std::abort();
    }
  }

  utility::umap_fits_file::PerFits_free_cube(cube.data);

  std::cout << "Passed all tests" << std::endl;

  return 0;
}
