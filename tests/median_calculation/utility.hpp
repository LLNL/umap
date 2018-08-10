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

/// Memo about median calculation
/// x: horizontal dimension of a frame at a certain time point
/// y: vertical dimension of a frame at a certain time point
/// k: time dimension
/// A cube is a set of 'k' frames

#ifndef MEDIAN_CALCULATION_COMMON_HPP
#define MEDIAN_CALCULATION_COMMON_HPP

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <cmath>
#include <cfenv>
#include <cassert>

#define MEDIAN_CALCULATION_COLUMN_MAJOR 1
// #define MEDIAN_CALCULATION_VERBOSE_OUT_OF_RANGE

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
inline ssize_t get_index(const cube_t<pixel_type>& cube, const size_t x, const size_t y, const size_t k)
{
#if MEDIAN_CALCULATION_COLUMN_MAJOR
  const ssize_t frame_index = x + y * cube.size_x;
#else
  const ssize_t frame_index = x * cube.size_y + y;
#endif

  if (get_frame_size(cube) <= frame_index) {
#ifdef MEDIAN_CALCULATION_VERBOSE_OUT_OF_RANGE
    std::cerr << "Frame index is out-of-range: "
              << "(" << x << ", " << y << ") is out of "
              << "(" << cube.size_x << ", " << cube.size_y << ")" << std::endl;
#endif
    return -1;
  }

  const ssize_t cube_index = frame_index + k * get_frame_size(cube);

  if (get_cube_size(cube) <= cube_index) {
#ifdef MEDIAN_CALCULATION_VERBOSE_OUT_OF_RANGE
    std::cerr << "Cube index is out-of-range: "
              << "(" << x << ", " << y << ", " << k << ") is out of "
              << "(" << cube.size_x << ", " << cube.size_y << ", " << cube.size_k << ")" << std::endl;
#endif
    return -1;
  }

  return cube_index;
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

template <typename pixel_type>
bool is_nan(const pixel_type value)
{
  return std::isnan(value);
}

} // namespace median

#endif //MEDIAN_CALCULATION_COMMON_HPP
