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

#include "median_calculation_common.hpp"
#include "torben.hpp"
#include "testoptions.h"
#include "PerFits.h"

const float x_intercept[6] = {1058.2, 1325.606, 1010.564, 829.674, 1390.826, 1091.015};
const float x_slope[6] = {3.5, 3.5, 3.5, 3.5, 3.5, 3.5};
const float y_intercept[6] = {124, 424, 724, 1024, 1324, 1624};
const float y_slope[6] = {0, 0, 0, 0, 0, 0};

// Iterator class to use torben function with vector model
// This class is a minimum implementation of an iterator to use the torben function
class vector_iterator {
 public:
  // Required types to use some stl functions
  using value_type = pixel_type;
  using difference_type = ssize_t;
  using iterator_category = std::random_access_iterator_tag;
  using pointer = value_type *;
  using reference = value_type &;

  // Constructor
  vector_iterator(const median::cube_t<pixel_type> &_cube,
                         const median::vector_t &_vector,
                         const size_t _start_pos)
      : cube(_cube),
        vector(_vector),
        current_pos(_start_pos) {}

  // Use default copy constructor
  vector_iterator(const vector_iterator&) = default;

  // To support
  // iterator1 != iterator2
  bool operator!=(const vector_iterator &other) {
    return current_pos != other.current_pos;
  }

  // To support
  // difference_type diff = iterator2 - iterator1
  difference_type operator-(const vector_iterator &other) {
    return current_pos - other.current_pos;
  }

  // To support
  // value_type val = *iterator
  value_type operator*() {
    return median::reverse_byte_order(cube.data[median::get_index(cube, vector, current_pos)]);
  }

  // To support
  // value_type val = iterator[1]
  value_type operator[](size_t pos) {
    return median::reverse_byte_order(cube.data[median::get_index(cube, vector, pos)]);
  }

  // To support
  // ++iterator
  value_type operator++() {
    size_t tmp = current_pos;
    ++current_pos;
    return (*this)[tmp];
  }

  median::cube_t<pixel_type> cube;
  median::vector_t vector;
  size_t current_pos;
};

int main(int argc, char** argv)
{
  umt_optstruct_t options;
  umt_getoptions(&options, argc, argv);

  size_t BytesPerElement;
  median::cube_t<float> cube;

  cube.data = (float*)PerFits::PerFits_alloc_cube(&options, &BytesPerElement, &cube.size_x, &cube.size_y, &cube.size_k);

  for (int i = 0; i < 6; ++i) {
    std::cout << "\nEpoch: " << i << std::endl;
    median::vector_t vector{x_intercept[i], x_slope[i], y_intercept[i], y_slope[i]};
    vector_iterator begin(cube, vector, 0);
    vector_iterator end(cube, vector, cube.size_k);
    const auto median_val = median::torben(begin, end);
    std::cout << "median value: " << median_val << std::endl;
  }

  PerFits::PerFits_free_cube(cube.data);

  return 0;
}
