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


#ifndef UMAP_VECTOR_HPP
#define UMAP_VECTOR_HPP

#include <iostream>
#include <random>
#include <vector>
#include <algorithm>
#include <cassert>

#include "utility.hpp"

struct vector_t
{
  double x_intercept;
  double x_slope;
  double y_intercept;
  double y_slope;
};


/// \brief Returns an index of a 3D coordinate. vector version
template <typename pixel_type>
inline ssize_t get_index(const median::cube_t<pixel_type>& cube, const vector_t& vec, const size_t epoch)
{
  return median::get_index(cube,
                           std::round(vec.x_slope * epoch + vec.x_intercept),
                           std::round(vec.y_slope * epoch + vec.y_intercept),
                           epoch);
}

// Iterator class to use torben function with vector model
// This class is a minimum implementation of an iterator to use the torben function
template <typename pixel_type>
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
                  const vector_t &_vector,
                  const size_t _start_pos)
      : cube(_cube),
        vector(_vector),
        current_pos(_start_pos) {
    // Note that when 'end' iterator is given to copy-constructor and MEDIAN_CALCULATION_VERBOSE_OUT_OF_RANGE is ON,
    // this code will trigger the out-of-range error message in get_index() function
    // although passing an 'end' iterator to copy-constructor is an expected behaivor
    if (is_out_of_range(current_pos)) {
      move_to_end();
      return;
    }

    const value_type current_value = *(*this);
    if (median::is_nan(current_value)) {
      move_to_next_valid_position();
    }
  }

  // Use default copy constructor
  vector_iterator(const vector_iterator&) = default;

  // To support
  // iterator1 == iterator2
  bool operator==(const vector_iterator &other) {
    return current_pos == other.current_pos;
  }

  // To support
  // iterator1 != iterator2
  bool operator!=(const vector_iterator &other) {
    return !(*this == other);
  }

  // To support
  // difference_type diff = iterator2 - iterator1
  difference_type operator-(const vector_iterator &other) {
    return current_pos - other.current_pos;
  }

  // To support
  // value_type val = *iterator
  value_type operator*() {
    assert(!is_out_of_range(current_pos)); // for sanitary check
    return median::reverse_byte_order(cube.data[get_index(cube, vector, current_pos)]);
  }

  // To support
  // ++iterator
  vector_iterator& operator++() {
    move_to_next_valid_position();
    return (*this);
  }

  // Utility function returns an iterator object pointing the end
  static vector_iterator create_end(const median::cube_t<pixel_type> &cube, const vector_t &vector) {
    vector_iterator iterator(cube, vector, 0); // 0 is a dummy value
    iterator.move_to_end();
    return iterator;
  }

 private:
  void move_to_next_valid_position() {
    ++current_pos;

    // Skip 'nan' values
    // When the vector is outside of the cube, move to the 'end' position
    while (true) {
      if (cube.size_k <= current_pos) {
        move_to_end();
        break;
      }

      if (is_out_of_range(current_pos)) {
        move_to_end();
        break;
      }

      const value_type current_value = *(*this);
      if (!median::is_nan(current_value)) {
        break; // Found next non-'nan' value
      }

      ++current_pos;
    }
  }

  // move position to the 'end' position
  void move_to_end() {
    current_pos = cube.size_k;
  }

  bool is_out_of_range(const size_t pos) {
    return (get_index(cube, vector, pos) == -1);
  }

  median::cube_t<pixel_type> cube;
  vector_t vector;
  size_t current_pos;
};

#endif //UMAP_VECTOR_HPP
