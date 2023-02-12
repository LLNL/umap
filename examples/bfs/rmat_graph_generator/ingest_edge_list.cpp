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
#include <fcntl.h>
#include <sys/mman.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <tuple>
#include <cstring>

#include "utility/bitmap.hpp"
#include "utility/mmap.hpp"
#include "utility/file.hpp"

std::pair<uint64_t, uint64_t>
check_edge_list(const std::vector<std::string> &edge_list_file_names) {
  uint64_t max_id = 0;
  size_t num_edges = 0;

  std::cout << "---------- Find max vertex ID and count #of edges ----------" << std::endl;
#ifdef _OPENMP
#pragma omp parallel for reduction(+:num_edges), reduction(max : max_id)
#endif
  for (size_t i = 0; i < edge_list_file_names.size(); ++i) {
    std::ifstream ifs(edge_list_file_names[i]);
    if (!ifs.is_open()) {
      std::cerr << "Can not open " << edge_list_file_names[i] << std::endl;
      std::abort();
    }
    std::cout << "Reading " << edge_list_file_names[i] << std::endl;

    uint64_t src, trg;
    while (ifs >> src >> trg) {
      max_id = std::max(src, max_id);
      max_id = std::max(trg, max_id);
      ++num_edges;
    }
  }

  return std::make_pair(max_id, num_edges);
}

void ingest_edges(void *const map_raw_address, const uint64_t max_id, const std::vector<std::string> &edge_list_file_names) {

  uint64_t *const index = new uint64_t[max_id + 2];
  for (size_t i = 0; i < max_id + 2; ++i) index[i] = 0;
  std::cout << static_cast<double>((max_id + 2) * sizeof(uint64_t)) / (1ULL << 30)
            << " GB is allocated for index" << std::endl;

  std::cout << "---------- Count degree ----------" << std::endl;
#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (size_t i = 0; i < edge_list_file_names.size(); ++i) {

#ifdef _OPENMP
#pragma omp critical
#endif
    {
      std::cout << "Reading " << edge_list_file_names[i] << std::endl;
    }

    std::ifstream edge_stream(edge_list_file_names[i]);
    if (!edge_stream.is_open()) {
      std::cerr << "Can not open " << edge_list_file_names[i] << std::endl;
      std::abort();
    }

    uint64_t source, target;
    while (edge_stream >> source >> target) {
#ifdef _OPENMP
#pragma omp atomic
#endif
      ++index[source];
    }
  }

  std::cout << "---------- Construct index ----------" << std::endl;
  for (size_t i = max_id + 1; i > 0; --i) {
    index[i] = index[i - 1];
  }
  index[0] = 0;
  for (size_t i = 0; i < max_id + 1; ++i) {
    index[i + 1] += index[i];
  }

  std::cout << "---------- Writing index ----------" << std::endl;
  uint64_t *index_map = static_cast<uint64_t *>(map_raw_address);
  std::memcpy(index_map, index, (max_id + 2) * sizeof(uint64_t));

  std::cout << "---------- Load edges ----------" << std::endl;
  const std::ptrdiff_t edges_offset = static_cast<std::ptrdiff_t>(max_id) + 2;
  uint64_t *edges_map = static_cast<uint64_t *>(map_raw_address) + edges_offset;

#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (size_t i = 0; i < edge_list_file_names.size(); ++i) {

#ifdef _OPENMP
#pragma omp critical
#endif
    {
      std::cout << "Reading " << edge_list_file_names[i] << std::endl;
    }

    std::ifstream edge_stream(edge_list_file_names[i]);

    uint64_t source, target;
    while (edge_stream >> source >> target) {
      size_t pos;
#ifdef _OPENMP
#pragma omp atomic capture
#endif
      pos = index[source]++;
      edges_map[pos] = target;
    }
  }

  delete[] index;
}

void parse_options(int argc, char **argv,
                   std::string &graph_file_name,
                   std::vector<std::string> &edge_list_file_names) {
  graph_file_name = "";

  int c;
  while ((c = getopt(argc, argv, "g:h")) != -1) {
    switch (c) {
      case 'g': /// Required
        graph_file_name = optarg;
        break;

      case 'h':
        // usage();
        break;
    }
  }

  for (int index = optind; index < argc; index++) {
    edge_list_file_names.emplace_back(argv[index]);
  }
}

int main(int argc, char **argv) {
  std::string graph_file_name;
  std::vector<std::string> edge_list_file_names;

  parse_options(argc, argv, graph_file_name, edge_list_file_names);

  uint64_t max_id;
  size_t num_edges;
  std::tie(max_id, num_edges) = check_edge_list(edge_list_file_names);
  std::cout << "num_vertices: " << max_id + 1 << std::endl;
  std::cout << "num_edges: " << num_edges << std::endl;

  /// ----- create and map output (graph) file ----- ///
  const size_t graph_size = (max_id + 2 + num_edges) * sizeof(uint64_t);
  if (!utility::create_file(graph_file_name)) {
    std::cerr << "Failed to create a file: " << graph_file_name << std::endl;
    std::abort();
  }
  if (!utility::extend_file_size(graph_file_name, graph_size)) {
    std::cerr << "Failed to extend a file: " << graph_file_name << std::endl;
    std::abort();
  }

  int fd = -1;
  void *map_raw_address = nullptr;
  std::tie(fd, map_raw_address) = utility::map_file_write_mode(graph_file_name, nullptr, graph_size, 0);
  if (fd == -1 || map_raw_address == nullptr) {
    std::cerr << "Failed to map a file with write mode: " << graph_file_name << std::endl;
    std::abort();
  }

  /// ----- ingest edges ----- ///
  ingest_edges(map_raw_address, max_id, edge_list_file_names);

  /// ----- closing ----- ///
  utility::munmap(map_raw_address, graph_size, true);
  std::cout << "Edge list ingestion is finished" << std::endl;

  return 0;
}
