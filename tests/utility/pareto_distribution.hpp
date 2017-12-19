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

#ifndef UTILITY_PARETO_DISTRIBUTION_HPP
#define UTILITY_PARETO_DISTRIBUTION_HPP

#include <random>
#include <chrono>
#include <cassert>

namespace utility
{

/// \brief Generate random numbers [0, max_t) with pareto distribution
class pareto_int_distribution
{
 public:
  pareto_int_distribution(unsigned seed, double alpha, double am, uint64_t max_t)
    : m_rnd_gen(seed),
      m_uniform_dist(0.0, 1.0),
      m_alpha(alpha),
      m_am(am),
      m_max_t(max_t) {}

  pareto_int_distribution(const pareto_int_distribution&) = delete;
  pareto_int_distribution(pareto_int_distribution&&) = default;
  pareto_int_distribution& operator=(const pareto_int_distribution&) = delete;
  pareto_int_distribution& operator=(pareto_int_distribution&&) = default;

  uint64_t gen()
  {
    uint64_t safe_counter = 0;

    while (true) {
      const auto t = static_cast<uint64_t>(m_am / std::pow(m_uniform_dist(m_rnd_gen), 1.0 / m_alpha)) - 1.0;
      if (t < m_max_t)
        return t;

      ++safe_counter;
      assert(safe_counter < (1 << 20));
    }
  }

 private:
  std::mt19937_64 m_rnd_gen;
  std::uniform_real_distribution<double> m_uniform_dist;
  double m_alpha;
  double m_am;
  uint64_t m_max_t;
};

} // namespace utility
#endif // UTILITY_PARETO_DISTRIBUTION_HPP
