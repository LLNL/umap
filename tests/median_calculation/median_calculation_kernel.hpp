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

/// The original code of Torben algorithm is in public domain
/// Its algorithm is developed by Torben Mogensen and implementated by N. Devillard
/// This version considerably modified
/// This implementation also contains the modification proposed in https://github.com/sarnold/medians-1D/issues/8

/// Memo about median calculation
/// x: horizontal dimension of a frame at a certain time point
/// y: vertical dimension of a frame at a certain time point
/// k: time dimension
/// A cube is a set of 'k' frames

#ifndef MEDIAN_CALCULATION_KERNEL_HPP
#define MEDIAN_CALCULATION_KERNEL_HPP

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <cmath>
#include <cfenv>
#include <vector>

#define MEDIAN_CALCULATION_VERBOSE 0
#define MEDIAN_CALCULATION_COLUMN_MAJOR 1

namespace median
{

template <typename _pixel_type>
struct cube_t {
  using pixel_type = _pixel_type;

  size_t size_x;
  size_t size_y;
  size_t size_k;
  pixel_type *data;
};

struct vector_t
{
  double x_intercept;
  double x_slope;
  double y_intercept;
  double y_slope;
};


/// \brief Return frame size
template <typename pixel_type>
inline size_t get_frame_size(const cube_t<pixel_type>& cube)
{
  return cube.size_x * cube.size_y;
}

/// \brief Return cube size
template <typename pixel_type>
inline size_t get_cube_size(const cube_t<pixel_type>& cube)
{
  return cube.size_x * cube.size_y * cube.size_k;
}

/// \brief Returns an index of a 3D coordinate
template <typename pixel_type>
inline size_t get_index(const cube_t<pixel_type>& cube, const size_t x, const size_t y, const size_t k)
{
#if MEDIAN_CALCULATION_COLUMN_MAJOR
  return x + y * cube.size_x + k * get_frame_size(cube); // column major
#else
  return x * cube.size_y + y + k * get_frame_size(cube); // row major
#endif
}

/// \brief Returns an index of a 3D coordinate. vector version
template <typename pixel_type>
inline size_t get_index(const cube_t<pixel_type>& cube, const vector_t& vec, const size_t epoch)
{
  return get_index(cube,
                   std::round(vec.x_slope * epoch + vec.x_intercept),
                   std::round(vec.y_slope * epoch + vec.y_intercept),
                   epoch);
}

/// \brief Reverses byte order
/// \tparam T Type of value; currently only 4 Byte types are supported
/// \param x Input value
/// \return Given value being reversed byte order
template <typename T>
inline T reverse_byte_order(const T x)
{
  static_assert(sizeof(T) == 4, "T is not a 4 byte of type");
  T reversed_x;
  auto *const p1 = reinterpret_cast<const char *>(&x);
  auto *const p2 = reinterpret_cast<char *>(&reversed_x);
  p2[0] = p1[3];
  p2[1] = p1[2];
  p2[2] = p1[1];
  p2[3] = p1[0];

  return reversed_x;
}

/// \brief Data structure to store a read value
template <typename pixel_type>
struct read_value_t {
  size_t pos_x;
  size_t pos_y;
  pixel_type value;

  read_value_t(size_t _pos_x, size_t _pos_y, pixel_type _value)
      : pos_x(_pos_x),
        pos_y(_pos_y),
        value(_value) {}
};

/// \example
///  cube = cube_t<float>{10, 10, 10, ptr_cube};
///  vector_t vector{10, 2.3, 10, 1.0};
///  auto ret = median::torben(cube, vector);
/// ret is a pair of pair of a calculated median value and an array of read pixel values
template <typename pixel_type>
std::pair<pixel_type, std::vector<read_value_t<pixel_type>>>
torben(const cube_t<pixel_type>& cube, const vector_t& vector)
{
  pixel_type min;
  pixel_type max;

  std::vector<read_value_t<pixel_type>> read_values;

  // get a value of the starting point
  min = max = reverse_byte_order(cube.data[get_index(cube, vector, 0)]);

  // ---------- Find min and max value over time frame ---------- //
  for (size_t k = 0; k < cube.size_k; ++k) {
    const size_t pos = get_index(cube, vector, k);
    const pixel_type value = reverse_byte_order(cube.data[pos]);
    min = std::min(min, value);
    max = std::max(max, value);
  }

  // ---------- Find median value ---------- //
  size_t less, greater, equal;
  pixel_type guess, maxltguess, mingtguess;
  size_t loop_cnt = 0;
  while (true) {
    guess = (min + max) / 2.0; // Should cast to double before divide?
    less = 0;
    greater = 0;
    equal = 0;
    maxltguess = min;
    mingtguess = max;

    for (size_t k = 0; k < cube.size_k; ++k) {
      const size_t pos = get_index(cube, vector, k);
      const pixel_type value = reverse_byte_order(cube.data[pos]);
      if (loop_cnt == 0) { // store data to return to the main function
        const size_t pos_x = std::round(vector.x_slope * k + vector.x_intercept);
        const size_t pos_y = std::round(vector.y_slope * k + vector.y_intercept);
        read_values.emplace_back(read_value_t<pixel_type>(pos_x, pos_y, value));
#if MEDIAN_CALCULATION_VERBOSE
        std::cout << pos_x << "\t" << pos_y << "\t" << value << std::endl;
#endif
      }
      if (value < guess) {
        less++;
        if (value > maxltguess) maxltguess = value;
      } else if (value > guess) {
        greater++;
        if (value < mingtguess) mingtguess = value;
      } else {
        equal++;
      }
    }

    const size_t half = (cube.size_k + 1) / 2;
    if (less <= half && greater <= half) break;
    else if (less > greater) max = maxltguess;
    else min = mingtguess;

    ++loop_cnt;
  }

  // ----- Calculate a mean value if cube.size_k is an even number ----- //
  const size_t half = (cube.size_k + 1) / 2;

  if (less >= half) min = maxltguess;
  else if (less + equal >= half) min = guess;
  else min = mingtguess;

  if (cube.size_k & 1) return std::make_pair(min, read_values);

  if (greater >= half) max = mingtguess;
  else if (greater + equal >= half) max = guess;
  else max = maxltguess;

  return std::make_pair((min + max) / 2.0, read_values);
}
} // namespace median

#endif //MEDIAN_CALCULATION_KERNEL_HPP
