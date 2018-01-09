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

#ifndef UTILITY_UNIFORM_DISTRIBUTION_HPP
#define UTILITY_UNIFORM_DISTRIBUTION_HPP

#include <random>
#include <chrono>
#include <cassert>
#include <type_traits>

namespace utility
{

/// \brief Generate random numbers [min_n, max_n) using a uniform distribution
template <typename T>
class uniform_distribution
{
 public:
  /// \brief  Select a uniform_distribution class
  /// For integral types, uniform_distribution is selected
  /// For floating point types, uniform_real_distribution is selected
  /// Otherwise, std::nullptr_t is selected
  using uniform_distribution_type = typename std::conditional<std::is_integral<T>::value,
                                                              std::uniform_int_distribution<T>,
                                                              typename std::conditional<std::is_floating_point<T>::value,
                                                                                        std::uniform_real_distribution<T>,
                                                                                        std::nullptr_t
                                                                                       >::type
                                                              >::type;


  uniform_distribution(unsigned seed, T min_n, T max_n)
    : m_rnd_gen(seed),
      m_uniform_dist(min_n, max_n) { }

  uniform_distribution(const uniform_distribution &) = delete;
  uniform_distribution(uniform_distribution &&) = default;
  uniform_distribution &operator=(const uniform_distribution &) = delete;
  uniform_distribution &operator=(uniform_distribution &&) = default;

  T gen()
  {
    return m_uniform_dist(m_rnd_gen);
  }

 private:
  std::mt19937_64 m_rnd_gen;
  uniform_distribution_type m_uniform_dist;
};

} // namespace utility
#endif //UTILITY_UNIFORM_DISTRIBUTION_HPP
