/*
 * Copyright (c) 2013, Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory.
 * Written by Roger Pearce <rpearce@llnl.gov>.
 * LLNL-CODE-644630.
 * All rights reserved.
 *
 * This file is part of HavoqGT, Version 0.1.
 * For details, see https://computation.llnl.gov/casc/dcca-pub/dcca/Downloads.html
 *
 * Please also read this link – Our Notice and GNU Lesser General Public License.
 *   http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (as published by the Free
 * Software Foundation) version 2.1 dated February 1999.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * OUR NOTICE AND TERMS AND CONDITIONS OF THE GNU GENERAL PUBLIC LICENSE
 *
 * Our Preamble Notice
 *
 * A. This notice is required to be provided under our contract with the
 * U.S. Department of Energy (DOE). This work was produced at the Lawrence
 * Livermore National Laboratory under Contract No. DE-AC52-07NA27344 with the DOE.
 *
 * B. Neither the United States Government nor Lawrence Livermore National
 * Security, LLC nor any of their employees, makes any warranty, express or
 * implied, or assumes any liability or responsibility for the accuracy,
 * completeness, or usefulness of any information, apparatus, product, or process
 * disclosed, or represents that its use would not infringe privately-owned rights.
 *
 * C. Also, reference herein to any specific commercial products, process, or
 * services by trade name, trademark, manufacturer or otherwise does not
 * necessarily constitute or imply its endorsement, recommendation, or favoring by
 * the United States Government or Lawrence Livermore National Security, LLC. The
 * views and opinions of authors expressed herein do not necessarily state or
 * reflect those of the United States Government or Lawrence Livermore National
 * Security, LLC, and shall not be used for advertising or product endorsement
 * purposes.
 *
 */

#ifndef BFS_RMAT_EDGE_GENERATOR_HPP
#define BFS_RMAT_EDGE_GENERATOR_HPP

#include <cstddef>
#include <iostream>
#include <random>
#include <cassert>
#include "hash.hpp"

#include <utility>

/// RMAT edge generator, based on Boost Graph's RMAT generator
///
/// Options include scrambling vertices based on a hash funciton, and
/// symmetrizing the list.   Generated edges are not sorted.  May contain
/// duplicate and self edges.
class rmat_edge_generator {

 public:
  typedef uint64_t vertex_descriptor;
  typedef std::pair<uint64_t, uint64_t> value_type;
  typedef value_type edge_type;

  ///
  /// InputIterator class for rmat_edge_generator

  class input_iterator_type : public std::iterator<std::input_iterator_tag,
                                                   edge_type,
                                                   std::ptrdiff_t,
                                                   const edge_type *,
                                                   const edge_type &> {

   public:
    input_iterator_type(rmat_edge_generator *ptr_rmat, uint64_t count)
        : m_ptr_rmat(ptr_rmat), m_rng(ptr_rmat->m_seed), m_dis(0.0, 1.0), m_count(count), m_make_undirected(false) {
      if (m_count == 0) {
        get_next();
        m_count = 0; //reset to zero
      }
    }

    const edge_type &operator*() const { return m_current; }
    //const uint64_t* operator->() const { return &(operator*()); }
    input_iterator_type &operator++() {
      get_next();
      return *this;
    }

    input_iterator_type operator++(int) {
      input_iterator_type __tmp = *this;
      get_next();
      return __tmp;
    }

    edge_type *operator->() {
      return &m_current;
    }

    bool is_equal(const input_iterator_type &_x) const {
      return m_count == (_x.m_count);
    }

    ///  Return true if x and y are both end or not end, or x and y are the same.
    friend bool
    operator==(const input_iterator_type &x, const input_iterator_type &y) { return x.is_equal(y); }

    ///  Return false if x and y are both end or not end, or x and y are the same.
    friend bool
    operator!=(const input_iterator_type &x, const input_iterator_type &y) { return !x.is_equal(y); }

   private:
    input_iterator_type();

    void get_next() {
      if (m_ptr_rmat->m_undirected && m_make_undirected) {
        std::swap(m_current.first, m_current.second);
        m_make_undirected = false;
      } else {
        m_current = m_ptr_rmat->generate_edge(m_rng, m_dis);
        ++m_count;
        m_make_undirected = true;
      }
      assert(m_current.first <= m_ptr_rmat->max_vertex_id());
      assert(m_current.second <= m_ptr_rmat->max_vertex_id());
    }

    rmat_edge_generator *m_ptr_rmat;
    std::mt19937 m_rng;
    std::uniform_real_distribution<> m_dis;
    uint64_t m_count;
    edge_type m_current;
    bool m_make_undirected;
  };

  /// seed used to be 5489
  rmat_edge_generator(uint64_t seed, uint64_t vertex_scale,
                      uint64_t edge_count, double a, double b, double c,
                      double d, bool scramble, bool undirected)
      : m_seed(seed),
        m_rng(seed),
        m_dis(0.0, 1.0),
        m_vertex_scale(vertex_scale),
        m_edge_count(edge_count),
        m_scramble(scramble),
        m_undirected(undirected),
        m_rmat_a(a),
        m_rmat_b(b),
        m_rmat_c(c),
        m_rmat_d(d) {

    //  sanity_max_vertex_id();

    // #ifdef DEBUG
    //   auto itr1 = begin();
    //   auto itr2 = begin();
    //   while (itr1 != end()) {
    //     assert(itr2 != end());
    //     assert(*itr1 == *itr2);
    //     assert(itr1 == itr2);
    //     itr1++;
    //     itr2++;
    //   }
    // #endif
  }

  /// Returns the begin of the input iterator
  input_iterator_type begin() {
    return input_iterator_type(this, 0);
  }

  /// Returns the end of the input iterator
  input_iterator_type end() {
    return input_iterator_type(this, m_edge_count);
  }

  void sanity_max_vertex_id() {
    auto itr = begin();
    auto itr_end = end();

    uint64_t value = 0;
    while (itr != itr_end) {
      value = std::max(value, (*itr).first);
      value = std::max(value, (*itr).second);
    }

    std::cout << " value: " << value << std::endl;
    assert(max_vertex_id() == value);

  }
  uint64_t max_vertex_id() {
    return (uint64_t(1) << uint64_t(m_vertex_scale)) - 1;
  }

  size_t size() {
    return m_edge_count;
  }

 protected:
  /// Generates a new RMAT edge.  This function was adapted from the Boost Graph Library.
  edge_type generate_edge() {
    return generate_edge(m_rng, m_dis);
  }
  edge_type generate_edge(std::mt19937& rng, std::uniform_real_distribution<> &dis) {
    double rmat_a = m_rmat_a;
    double rmat_b = m_rmat_b;
    double rmat_c = m_rmat_c;
    double rmat_d = m_rmat_d;
    uint64_t u = 0, v = 0;
    uint64_t step = (uint64_t(1) << m_vertex_scale) / 2;
    for (unsigned int j = 0; j < m_vertex_scale; ++j) {
      double p = dis(rng);

      if (p < rmat_a);
      else if (p >= rmat_a && p < rmat_a + rmat_b)
        v += step;
      else if (p >= rmat_a + rmat_b && p < rmat_a + rmat_b + rmat_c)
        u += step;
      else { // p > a + b + c && p < a + b + c + d
        u += step;
        v += step;
      }

      step /= 2;

      // 0.2 and 0.9 are hardcoded in the reference SSCA implementation.
      // The maximum change in any given value should be less than 10%
      rmat_a *= 0.9 + 0.2 * dis(rng);
      rmat_b *= 0.9 + 0.2 * dis(rng);
      rmat_c *= 0.9 + 0.2 * dis(rng);
      rmat_d *= 0.9 + 0.2 * dis(rng);

      double S = rmat_a + rmat_b + rmat_c + rmat_d;

      rmat_a /= S;
      rmat_b /= S;
      rmat_c /= S;
      // d /= S;
      // Ensure all values add up to 1, regardless of floating point errors
      rmat_d = 1. - rmat_a - rmat_b - rmat_c;
    }
    if (m_scramble) {
      u = rmat_edge_generator_detail::hash_nbits(u, m_vertex_scale);
      v = rmat_edge_generator_detail::hash_nbits(v, m_vertex_scale);
    }

    return std::make_pair(u, v);
  }

  const uint64_t m_seed;
  std::mt19937 m_rng;
  std::uniform_real_distribution<> m_dis;
  const uint64_t m_vertex_scale;
  const uint64_t m_edge_count;
  const bool m_scramble;
  const bool m_undirected;
  const double m_rmat_a;
  const double m_rmat_b;
  const double m_rmat_c;
  const double m_rmat_d;
};

#endif //BFS_RMAT_EDGE_GENERATOR_HPP
