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

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

/// This is a utility program to compute a degree distribution
/// This program treat the input files as directed graph
/// Usage:
/// ./compute_degree_distribution [out file name] [edge list file names]
int main(int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "Wrong number of arguments" << std::endl;
    std::abort();
  }

  std::string out_file_name(argv[1]);

  // -- Count degree -- //
  std::unordered_map<uint64_t, uint64_t> degree_table;
  for (int i = 2; i < argc; ++i) {
    std::ifstream input_edge_list(argv[i]);
    if (!input_edge_list.is_open()) {
      std::cerr << "Cannot open " << argv[i] << std::endl;
      continue;
    }

    uint64_t source;
    uint64_t destination;
    while (input_edge_list >> source >> destination) {
      if (degree_table.count(source) == 0) {
        degree_table[source] = 0;
      }
      ++degree_table[source];
    }
  }

  // -- Compute degree distribution table -- //
  std::unordered_map<uint64_t, uint64_t> degree_dist_table;
  for (const auto &item : degree_table) {
    const uint64_t degree = item.second;
    if (degree_dist_table.count(degree) == 0) {
      degree_dist_table[degree] = 0;
    }
    ++degree_dist_table[degree];
  }

  // -- Sort the degree distribution table -- //
  std::vector<std::pair<uint64_t, uint64_t>> sorted_degree_dist_table;
  for (const auto &item : degree_dist_table) {
    const uint64_t degree = item.first;
    const uint64_t count = item.second;
    sorted_degree_dist_table.emplace_back(degree, count);
  }
  std::sort(sorted_degree_dist_table.begin(), sorted_degree_dist_table.end(),
            [](const std::pair<uint64_t, uint64_t> &lh, const std::pair<uint64_t, uint64_t> &rh) {
              return (lh.first < rh.first); // Sort in the ascending order of degree
            });

  // -- Dump the sorted degree distribution table -- //
  std::ofstream ofs(out_file_name);
  if (!ofs.is_open()) {
    std::cerr << "Cannot open " << out_file_name << std::endl;
    std::abort();
  }
  ofs << "Degree\tCount" << std::endl;
  for (const auto &item : sorted_degree_dist_table) {
    const uint64_t degree = item.first;
    const uint64_t count = item.second;
    ofs << degree << " " << count << "\n";
  }
  ofs.close();

  std::cout << "Finished degree distribution computation" << std::endl;

  return 0;
}