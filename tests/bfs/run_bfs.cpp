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
#include <unistd.h>
#include <iostream>
#include <vector>
#include <tuple>
#include <string>
#include <fstream>

#include "bfs_kernel.hpp"
#include "../lib/utility/bitmap.hpp"
#include "../lib/utility/mmap.hpp"

void parse_options(int argc, char **argv,
                   size_t &num_vertices, size_t &num_edges,
                   std::string &graph_file_name) {
  num_vertices = 0;
  num_edges = 0;
  graph_file_name = "";

  char c;
  while ((c = getopt(argc, argv, "n:m:g:h")) != -1) {
    switch (c) {
      case 'n': /// Required
        num_vertices = std::stoull(optarg);
        break;

      case 'm': /// Required
        num_edges = std::stoull(optarg);
        break;

      case 'g': /// Required
        graph_file_name = optarg;
        break;

      case 'h':
        // usage();
        break;
    }
  }
}

std::pair<uint64_t *, uint64_t *>
map_graph(const size_t num_vertices, const size_t num_edges, const std::string &graph_file_name) {
  const size_t graph_size = (num_vertices + 1 + num_edges) * sizeof(uint64_t);

  int fd = -1;
  void *map_raw_address = nullptr;
  std::tie(fd, map_raw_address) = utility::map_file_read_mode(graph_file_name, nullptr, graph_size, 0);
  if (fd == -1 || map_raw_address == nullptr) {
    std::cerr << "Failed to map the graph" << std::endl;
    std::abort();
  }

  uint64_t *index = static_cast<uint64_t *>(map_raw_address);
  const std::ptrdiff_t edges_offset = num_vertices + 1;
  uint64_t *edges = static_cast<uint64_t *>(map_raw_address) + edges_offset;

  return std::make_pair(index, edges);
}

void find_bfs_root(const size_t num_vertices, const uint64_t *const index, uint16_t *const level) {
  for (uint64_t src = 0; src < num_vertices; ++src) {
    const size_t degree = index[src + 1] - index[src];
    if (degree > 0) {
      level[src] = 0;
      std::cout << "BFS root: " << src << std::endl;
      return;
    }
  }
  std::cerr << "Can not find a proper root vertex; all vertices do not have any edges?" << std::endl;
  std::abort();
}

void count_level(const size_t num_vertices, const uint16_t max_level, const uint16_t *const level) {

  std::vector<size_t> cnt(max_level + 1, 0);
  for (uint64_t i = 0; i < num_vertices; ++i) {
    if (level[i] == bfs::k_infinite_level) continue;
    if (level[i] > max_level) {
      std::cerr << "Invalid level: " << level[i] << " > " << max_level << std::endl;
      return;
    }
    ++cnt[level[i]];
  }

  std::cout << "Level\t#vertices" << std::endl;
  for (uint16_t i = 0; i <= max_level; ++i) {
    std::cout << i << "\t" << cnt[i] << std::endl;
  }
}

int main(int argc, char **argv) {
  size_t num_vertices;
  size_t num_edges;
  std::string graph_file_name;

  parse_options(argc, argv, num_vertices, num_edges, graph_file_name);

  const uint64_t *index = nullptr;
  const uint64_t *edges = nullptr;
  std::tie(index, edges) = map_graph(num_vertices, num_edges, graph_file_name);

  std::vector<uint16_t> level(num_vertices); // Array to store each vertex's level (a distance from the source vertex)
  std::vector<uint64_t> visited_filter(utility::bitmap_size(num_vertices)); // bitmap data to store 'visited' information

  bfs::init_bfs(num_vertices, level.data(), visited_filter.data());
  find_bfs_root(num_vertices, index, level.data());
  const uint16_t max_level = bfs::run_bfs(num_vertices, index, edges, level.data(), visited_filter.data());

  count_level(num_vertices, max_level, level.data());

  return 0;
}