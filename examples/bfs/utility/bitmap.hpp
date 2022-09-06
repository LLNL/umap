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
#ifndef LIB_UTILITY_BITMAP_HPP
#define LIB_UTILITY_BITMAP_HPP

#include <iostream>

namespace utility
{

/// examples
/// input 1 ~ 64 -> return 1;  input 65 ~ 128 -> return 2
inline constexpr size_t bitmap_size(const size_t size)
{
  return (size == 0) ? 0 :
         (size - 1ULL) / (sizeof(uint64_t) * 8ULL) + 1ULL;
}

/// examples
/// input 0 ~ 63 -> return 0; input 64 ~ 127 -> return 1;
inline constexpr uint64_t bitmap_global_pos(const uint64_t pos)
{
  return (pos >> 6ULL);
}

inline constexpr uint64_t bitmap_local_pos(const uint64_t pos)
{
  return pos & 0x3FULL;
}

inline bool get_bit(const uint64_t* const bitmap, const uint64_t pos)
{
  return static_cast<bool>(bitmap[bitmap_global_pos(pos)] & (0x1ULL << bitmap_local_pos(pos)));
}

inline void set_bit(uint64_t* const bitmap, const uint64_t pos)
{
  bitmap[bitmap_global_pos(pos)] |= 0x1ULL << bitmap_local_pos(pos);
}

} // namespace utility

#endif //LIB_UTILITY_BITMAP_HPP
