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
#include <fstream>
#include <string>

#include "../utility/time.hpp"
#include "../utility/openmp.hpp"
#include "../utility/mmap.hpp"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace median
{

/// \brief Reverse byte order
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

/// \brief Calculate median value of a pixel
/// \param cube A pointer pointing mapped cube
/// \param size_k Number of frames
/// \param frame_size Size of frame
/// \param xy_position XY position of the pixel to calculate median value
/// \return Median value of the pixel over time frame
template <typename value_type>
value_type torben(const value_type cube[], const size_t size_k, const size_t frame_size, const size_t xy_position)
{
  value_type min;
  value_type max;
  min = max = reverse_byte_order(cube[xy_position]);

  // ---------- Find min and max value over time frame ---------- //
  for (size_t k = 0; k < size_k; ++k) {
    const size_t pos = xy_position + k * frame_size;
    const value_type value = reverse_byte_order(cube[pos]);
    min = std::min(min, value);
    max = std::max(max, value);
  }

  // ---------- Find median value ---------- //
  size_t less, greater, equal;
  value_type guess, maxltguess, mingtguess;
  size_t loop_cnt = 0;
  while (true) {
    guess = (min + max) / 2.0; // Should cast to double before divide?
    less = 0;
    greater = 0;
    equal = 0;
    maxltguess = min;
    mingtguess = max;

    for (size_t k = 0; k < size_k; ++k) {
      const size_t pos = xy_position + k * frame_size;
      const value_type value = reverse_byte_order(cube[pos]);
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

    const size_t half = (size_k + 1) / 2;
    if (less <= half && greater <= half) break;
    else if (less > greater) max = maxltguess;
    else min = mingtguess;

    ++loop_cnt;
  }

  // ----- Calculate a mean value if size_k is a even number ----- //
  const size_t half = (size_k + 1) / 2;

  if (less >= half) min = maxltguess;
  else if (less + equal >= half) min = guess;
  else min = mingtguess;

  if (size_k & 1) return min;

  if (greater >= half) max = mingtguess;
  else if (greater + equal >= half) max = guess;
  else max = maxltguess;

  return (min + max) / 2.0;
}


/// \brief Calculate median value for a cubic array using OpenMP
/// \tparam value_type Type of an element
/// \param cube Pointer to the cube
/// \param size_x Width of a frame
/// \param size_y Height of a frame
/// \param size_k Number of frames
/// \param median_calculation_result Pointer to a 2D array to write results
template <typename value_type>
void calculate_median(const value_type *const cube,
                      const size_t size_x,
                      const size_t size_y,
                      const size_t size_k,
                      value_type *const median_calculation_result)
{
#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (size_t y = 0; y < size_y; ++y) {
    for (size_t x = 0; x < size_x; ++x) {
      const size_t frame_size = size_y * size_x;
      const size_t xy_position = y * size_x + x;
      median_calculation_result[x + y * size_x] = torben(cube, size_k, frame_size, xy_position);
    }
  }
}

} // namespace median
#endif //MEDIAN_CALCULATION_KERNEL_HPP
