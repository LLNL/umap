/*
 * This code is coming from the following project
 * /
/ *
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

#ifndef BFS_HASH_HPP
#define BFS_HASH_HPP

#include <cstdint>

namespace rmat_edge_generator_detail {


///
/// Hash functions
///
/// \todo requires documentation!
/// \todo requires testing!

inline uint32_t hash32(uint32_t a) {
  a = (a + 0x7ed55d16) + (a << 12);
  a = (a ^ 0xc761c23c) ^ (a >> 19);
  a = (a + 0x165667b1) + (a << 5);
  a = (a + 0xd3a2646c) ^ (a << 9);
  a = (a + 0xfd7046c5) + (a << 3);
  a = (a ^ 0xb55a4f09) ^ (a >> 16);
  return a;
}

inline uint16_t hash16(uint16_t a) {
  a = (a + 0x5d16) + (a << 6);
  a = (a ^ 0xc23c) ^ (a >> 9);
  a = (a + 0x67b1) + (a << 5);
  a = (a + 0x646c) ^ (a << 7);
  a = (a + 0x46c5) + (a << 3);
  a = (a ^ 0x4f09) ^ (a >> 8);
  return a;
}

inline uint64_t shifted_n_hash32(uint64_t input, int n) {
  uint64_t to_hash = input >> n;
  uint64_t mask = 0xFFFFFFFF;
  to_hash &= mask;
  to_hash = hash32(to_hash);

  to_hash <<= n;
  mask <<= n;
  //clear bits
  input &= ~mask;
  input |= to_hash;
  return input;
}

inline uint64_t shifted_n_hash16(uint64_t input, int n) {
  uint64_t to_hash = input >> n;
  uint64_t mask = 0xFFFF;
  to_hash &= mask;
  to_hash = hash16(to_hash);

  to_hash <<= n;
  mask <<= n;
  //clear bits
  input &= ~mask;
  input |= to_hash;
  return input;
}

inline uint64_t hash_nbits(uint64_t input, int n) {
  //std::cout << "hash_nbits(" << input << ", " << n << ") = ";
  if (n == 32) {
    input = hash32(input);
  } else if (n > 32) {
    assert(n > 32);
    n -= 32;
    for (int i = 0; i <= n; ++i) {
      input = shifted_n_hash32(input, i);
    }
    for (int i = n; i >= 0; --i) {
      input = shifted_n_hash32(input, i);
    }
  } else if (n < 32) {
    assert(n < 32);
    assert(n > 16 && "Hashing less than 16bits is not supported");
    n -= 16;
    for (int i = 0; i <= n; ++i) {
      input = shifted_n_hash16(input, i);
    }
    for (int i = n; i >= 0; --i) {
      input = shifted_n_hash16(input, i);
    }
  }
  //std::cout << input << std::endl;
  return input;
}

} // namespace rmat_edge_generator_detail
#endif //BFS_HASH_HPP
