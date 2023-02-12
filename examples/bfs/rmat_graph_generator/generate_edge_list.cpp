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
#include <fstream>
#include <string>
#include <vector>

#ifdef _OPENMP
#include<omp.h>
#endif

#include "rmat_edge_generator.hpp"

// ---------------------------------------- //
// Option
// ---------------------------------------- //
struct rmat_option_t {
  uint64_t seed{123};
  uint64_t vertex_scale{17};
  uint64_t edge_count{(1ULL << 17) * 16};
  double a{0.57};
  double b{0.19};
  double c{0.19};
  bool scramble_id{true};
  bool generate_both_directions{true};
};

bool parse_options(int argc, char **argv, rmat_option_t *option, std::string *out_edge_list_file_name) {
  int p;
  while ((p = getopt(argc, argv, "o:s:v:e:a:b:c:r:u:")) != -1) {
    switch (p) {
      case 'o':*out_edge_list_file_name = optarg; // required
        break;

      case 's':option->seed = std::stoull(optarg);
        break;

      case 'v':option->vertex_scale = std::stoull(optarg);
        break;

      case 'e':option->edge_count = std::stoull(optarg);
        break;

      case 'a':option->a = std::stod(optarg);
        break;

      case 'b':option->b = std::stod(optarg);
        break;

      case 'c':option->c = std::stod(optarg);
        break;

      case 'r':option->scramble_id = static_cast<bool>(std::stoi(optarg));
        break;

      case 'u':option->generate_both_directions = static_cast<bool>(std::stoi(optarg));
        break;

      default:std::cerr << "Illegal option" << std::endl;
        std::abort();
    }
  }

  if (out_edge_list_file_name->empty()) {
    std::cerr << "edge list file name (-o option) is required" << std::endl;
    std::abort();
  }

  std::cout << "seed: " << option->seed
            << "\nvertex_scale: " << option->vertex_scale
            << "\nedge_count: " << option->edge_count
            << "\na: " << option->a
            << "\nb: " << option->b
            << "\nc: " << option->c
            << "\nscramble_id: " << static_cast<int>(option->scramble_id)
            << "\ngenerate_both_directions: " << static_cast<int>(option->generate_both_directions)
            << "\nout_edge_list_file_name: " << *out_edge_list_file_name << std::endl;

  return true;
}

// ---------------------------------------- //
// Utility
// ---------------------------------------- //
int num_threads() {
#ifdef _OPENMP
  return omp_get_num_threads();
#else
  return 1;
#endif
}

int thread_num() {
#ifdef _OPENMP
  return omp_get_thread_num();
#else
  return 0;
#endif
}

// Compute the number of edges each thread generates
size_t num_local_edges(const size_t first, const size_t last, const size_t myid, const size_t nthreads) {
  size_t len = last - first + 1;
  size_t chunk = len / nthreads;
  size_t r = len % nthreads;

  size_t start;
  size_t end;

  if (myid < r) {
    start = first + (chunk + 1) * myid;
    end = start + chunk;
  } else {
    start = first + (chunk + 1) * r + chunk * (myid - r);
    end = start + chunk - 1;
  }

  return (end - start) + 1;
}

// ---------------------------------------- //
// Main
// ---------------------------------------- //
int main(int argc, char **argv) {

  rmat_option_t rmat_option;
  std::string out_edge_list_file_name;
  parse_options(argc, argv, &rmat_option, &out_edge_list_file_name);

  uint64_t num_generated_edges = 0;

#ifdef _OPENMP
#pragma omp parallel reduction(+:num_generated_edges)
#endif
  {
    const uint64_t num_edges_per_thread = num_local_edges(0, rmat_option.edge_count - 1, thread_num(), num_threads());
    const uint64_t seed = rmat_option.seed + thread_num();

    rmat_edge_generator rmat(seed, rmat_option.vertex_scale, num_edges_per_thread,
                             rmat_option.a, rmat_option.b, rmat_option.c,
                             1.0 - (rmat_option.a + rmat_option.b + rmat_option.c),
                             rmat_option.scramble_id, rmat_option.generate_both_directions);

    std::ofstream edge_list_file(out_edge_list_file_name + "_" + std::to_string(thread_num()));
    if (!edge_list_file.is_open()) {
      std::cerr << "Cannot open " << out_edge_list_file_name << std::endl;
      std::abort();
    }

    for (auto edge : rmat) {
      edge_list_file << edge.first << " " << edge.second << "\n";
      ++num_generated_edges;
    }
    edge_list_file.close();
  }

  // Sanity check
  const uint64_t num_edges_supposed_to_generate = (rmat_option.generate_both_directions) ? rmat_option.edge_count * 2ULL : rmat_option.edge_count;
  if (num_edges_supposed_to_generate != num_generated_edges) {
    std::cerr << "The number of generated edges is wrong"
              << "\n#edges generated: " << num_generated_edges
              << "\n#edges supposed to be generated: " << num_edges_supposed_to_generate << std::endl;
    std::abort();
  }

  std::cout << "Finished edge list generation" << std::endl;

  return 0;
}