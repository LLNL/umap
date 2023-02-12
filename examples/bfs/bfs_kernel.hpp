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
#ifndef BFS_BFS_KERNEL_HPP
#define BFS_BFS_KERNEL_HPP

#include <iostream>
#include <limits>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "utility/bitmap.hpp"
#include "utility/open_mp.hpp"

namespace bfs {

static const uint16_t k_infinite_level = std::numeric_limits<uint16_t>::max();

/// \brief initialize variables to run BFS
/// \param num_vertices The number of vertices
/// \param level A pointer of level array
/// \param A pointer to an bitset for visited filter
void init_bfs(const size_t num_vertices, uint16_t *const level, uint64_t *visited_filter) {
  for (size_t i = 0; i < num_vertices; ++i)
    level[i] = k_infinite_level;
  for (size_t i = 0; i < utility::bitmap_size(num_vertices); ++i)
    visited_filter[i] = 0;
}

/// \brief Print out the current omp configuration
void print_omp_configuration() {
#ifdef _OPENMP
#pragma omp parallel
  {
    if (::omp_get_thread_num() == 0)
      std::cout << "Run with " << ::omp_get_num_threads() << " threads" << std::endl;
  }
#else
  std::cout << "Run with a single thread" << std::endl;
#endif
}

/// \brief BFS kernel.
/// This kernel runs with OpenMP.
/// In order to simplify the implementation of this kernel,
/// some operations are not designed to avoid race conditions
/// as long as they are consistent with the correct status.
/// \param num_vertices The number of vertices
/// \param index A pointer of an index array
/// \param edges A pointer of an edges array
/// \param level A pointer of level array
/// \param visited_filter A pointer of an bitset for visited filter
uint16_t run_bfs(const size_t num_vertices,
                 const uint64_t *const index,
                 const uint64_t *const edges,
                 uint16_t *const level,
                 uint64_t *visited_filter) {

  print_omp_configuration();

  uint16_t current_level = 0;
  bool visited_new_vertex = false;

  while (true) { /// BFS main loop

    /// BFS loop for a single level
    /// We assume that the cost of generating threads at every level is negligible
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (uint64_t src = 0; src < num_vertices; ++src) { /// BFS loop for each level
      if (level[src] != current_level) continue;
      for (size_t i = index[src]; i < index[src + 1]; ++i) {
        const uint64_t trg = edges[i];
        if (!utility::get_bit(visited_filter, trg) && level[trg] == k_infinite_level) {
          level[trg] = current_level + 1;
          utility::set_bit(visited_filter, trg);
          visited_new_vertex = true;
        }
      }
    }

    if (!visited_new_vertex) break;

    ++current_level;
    printf("current_level = %d \n", current_level);
    visited_new_vertex = false;
  } /// End of BFS main loop

  return current_level;
}

}
#endif //BFS_BFS_KERNEL_HPP
