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
// Main
// ---------------------------------------- //
int main(int argc, char **argv) {

  rmat_option_t rmat_option;
  std::string out_edge_list_file_name;
  parse_options(argc, argv, &rmat_option, &out_edge_list_file_name);

  rmat_edge_generator rmat(rmat_option.seed, rmat_option.vertex_scale, rmat_option.edge_count,
                           rmat_option.a, rmat_option.b, rmat_option.c,
                           1.0 - (rmat_option.a + rmat_option.b + rmat_option.c),
                           rmat_option.scramble_id, rmat_option.generate_both_directions);

  std::ofstream edge_list_file(out_edge_list_file_name);
  if (!edge_list_file.is_open()) {
    std::cerr << "Cannot open " << out_edge_list_file_name << std::endl;
    std::abort();
  }

  for (auto edge : rmat) {
    edge_list_file << edge.first << " " << edge.second << "\n";
  }
  edge_list_file.close();

  std::cout << "Finished edge list generation" << std::endl;

  return 0;
}