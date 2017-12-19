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


#ifndef UTILITY_COMMON_HPP
#define UTILITY_COMMON_HPP

#include <cassert>

namespace utility
{

/// compute the next multiple of base
/// base is must be the power of 2
/// n is must be large than 0
inline constexpr size_t roundup(const size_t n, const size_t base)
{
  return ((n + (base - 1)) & (~(base - 1)));
}

/// \brief calculate the partial length each worker works on
/// \param length lenght
/// \param myid my id
/// \param num_workers the number of workers
/// \return begin and end index each worker works on. Note that [begin, end)
inline std::pair<size_t, size_t> cal_partial_range(const size_t length, const size_t myid, const size_t num_workers)
{
  size_t partial_length = length / num_workers;
  size_t r = length % num_workers;

  size_t begin_index;

  if (myid < r) {
    begin_index = (partial_length + 1) * myid;
    ++partial_length;
  } else {
    begin_index = (partial_length + 1) * r + partial_length * (myid - r);
  }

  return std::make_pair(begin_index, begin_index + partial_length);
}

inline size_t cal_partial_length(const size_t length, const size_t myid, const size_t num_workers)
{
  size_t partial_length = length / num_workers;
  size_t r = length % num_workers;

  if (myid < r) {
    ++partial_length;
  }

  return partial_length;
}

/// \brief calculate log2
/// n must be larger than 0
/// \param n
/// \return log2 of n
inline constexpr size_t cal_log2(const size_t n)
{
  return (n < 2) ? 0 : 1 + cal_log2(n / 2);
}

inline constexpr bool is_power_of_2(const size_t x)
{
  return x > 0 && !(x & (x - 1));
}

/// \brief return the next highest power of 2 value;
/// example: 5 -> 8; 16 -> 16
/// Note that if x is 0, this function returns 0
/// \param n input
/// \return the next highest power of 2 value
inline uint64_t cal_next_highest_power_of_2(const uint64_t n)
{
  uint64_t x = n;
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x |= x >> 32;
  x++;

  return x;
}

} // namespace utility

#endif // UTILITY_COMMON_HPP
