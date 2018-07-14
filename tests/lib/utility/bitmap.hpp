//
// Created by Iwabuchi, Keita on 6/22/18.
//

#ifndef LIB_UTILITY_BITMAP_HPP
#define LIB_UTILITY_BITMAP_HPP

#include <iostream>

namespace utility
{

/// examples
/// input 1 ~ 64 -> return 1;  input 65 ~ 128 -> return 2
constexpr size_t bitmap_size(const size_t size)
{
  return (size == 0) ? 0 :
         (size - 1ULL) / (sizeof(uint64_t) * 8ULL) + 1ULL;
}

/// examples
/// input 0 ~ 63 -> return 0; input 64 ~ 127 -> return 1;
constexpr uint64_t bitmap_global_pos(const uint64_t pos)
{
  return (pos >> 6ULL);
}

constexpr uint64_t bitmap_local_pos(const uint64_t pos)
{
  return pos & 0x3FULL;
}

bool get_bit(const uint64_t* const bitmap, const uint64_t pos)
{
  return bitmap[bitmap_global_pos(pos)] & (0x1ULL << bitmap_local_pos(pos));
}

void set_bit(uint64_t* const bitmap, const uint64_t pos)
{
  bitmap[bitmap_global_pos(pos)] |= 0x1ULL << bitmap_local_pos(pos);
}

} // namespace utility

#endif //LIB_UTILITY_BITMAP_HPP
