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
#include <random>
#include <chrono>
#include <vector>
#include <unistd.h>
#include <limits>

#include "median_calculation_kernel.hpp"
#include "../utility/mmap.hpp"

using value_type = float; // must be float or double

// -------------------------------------------------------------------------------- //
// Parse command line options
// -------------------------------------------------------------------------------- //
void usage()
{
  std::cout << "-x: size of width of a frame (required)\n"
            << "-y: size of height of a frame (required)\n"
            << "-k: number of frames (required)\n"
            << "-c: file name to store cube (required)\n"
            << "-m: file name to dump median values" << std::endl;
}

void parse_option(const int argc, char *const argv[],
                  size_t &size_x, size_t &size_y, size_t &size_k,
                  std::string &cube_file_name, std::string &median_value_file_name)
{
  bool option_x_found = false;
  bool option_y_found = false;
  bool option_k_found = false;
  bool option_c_found = false;

  int p;
  while ((p = ::getopt(argc, argv, "x:y:k:c:m:h")) != -1) {
    switch (p) {
      case 'x':
        size_x = std::stoull(optarg);
        option_x_found = true;
        break;

      case 'y':
        size_y = std::stoull(optarg);
        option_y_found = true;
        break;

      case 'k':
        size_k = std::stoull(optarg);
        option_k_found = true;
        break;

      case 'c':
        cube_file_name = optarg;
        option_c_found = true;
        break;

      case 'm':
        median_value_file_name = optarg;
        option_c_found = true;
        break;

      case 'h':
      default:
        usage();
        std::exit(0);
    }
  }

  if (!option_x_found || !option_y_found || !option_k_found || !option_c_found) {
    std::cerr << "Invalid arguments were given" << std::endl;
    usage();
    std::exit(1);
  }
}


// -------------------------------------------------------------------------------- //
// Generate cube
// -------------------------------------------------------------------------------- //
void generate_cube(const size_t size_x, const size_t size_y, const size_t size_k, const std::string& cube_file_name)
{
  const size_t cube_size = size_x * size_y * size_k * sizeof(value_type);

  auto ret = utility::create_and_map_file_write_mode(cube_file_name, nullptr, cube_size, 0);
  const int fd = ret.first;
  value_type *const cube = reinterpret_cast<value_type *>(ret.second);
  if (fd == -1 || cube == nullptr) {
    std::cerr << "Failed to map file: " << cube_file_name << std::endl;
    std::exit(1);
  }

  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
  std::mt19937_64 gen(seed);
  std::uniform_real_distribution<value_type> dis(0.0, std::numeric_limits<value_type>::max() / 4.0); //TODO: check reasonable value range

  for (size_t k = 0; k < size_k; ++k) {
    for (size_t y = 0; y < size_y; ++y) {
      for (size_t x = 0; x < size_x; ++x) {
        const value_type val = median::reverse_byte_order(dis(gen));
        cube[x + size_x * y + size_x * size_y * k] = val;
      }
    }
  }
  utility::munmap(fd, cube, cube_size);
}


// -------------------------------------------------------------------------------- //
// Dump median value
// -------------------------------------------------------------------------------- //
void dump_median_value(const size_t size_x, const size_t size_y, const size_t size_k,
                       const std::string& cube_file_name, const std::string& median_file_name)
{
  const size_t cube_size = size_x * size_y * size_k * sizeof(value_type);

  auto ret = utility::map_file_read_mode(cube_file_name, nullptr, cube_size, 0);
  const int fd = ret.first;
  value_type *const cube = reinterpret_cast<value_type *>(ret.second);
  if (fd == -1 || cube == nullptr) {
    std::cerr << "Failed to map file: " << cube_file_name << std::endl;
    std::exit(1);
  }

  std::ofstream of(median_file_name);
  if (!of.is_open()) {
    std::cerr << "Can not open: " << median_file_name << std::endl;
    std::exit(1);
  }

  std::vector<value_type> work;
  work.resize(size_k);

  // ----- Find median value ----- //
  for (size_t y = 0; y < size_y; ++y) {
    for (size_t x = 0; x < size_x; ++x) {
      for (size_t k = 0; k < size_k; ++k) {
        work[k] = median::reverse_byte_order(cube[x + size_x * y + size_x * size_y * k]);
      }
      std::sort(work.begin(), work.end());
      value_type median_value;
      if (size_k % 2 == 0) {
        median_value = static_cast<float>((work[size_k / 2 - 1] + work[size_k / 2]) / 2.0);
      } else {
        median_value = work[size_k / 2];
      }
      of << median_value << "\n";
    }
  }

  of.close();
  utility::munmap(fd, cube, cube_size);
}


int main(int argc, char *argv[])
{
  // -------------------- Initialization -------------------- //
  size_t size_x; // Width of a frame
  size_t size_y; // Height of a frame
  size_t size_k; // #frames
  std::string cube_file_name;
  std::string median_value_file_name;

  parse_option(argc, argv, size_x, size_y, size_k, cube_file_name, median_value_file_name);

  const size_t cube_size = size_x * size_y * size_k * sizeof(value_type);
  std::cout << "size (GB) = " << cube_size / static_cast<double>((1ULL << 30)) << std::endl;


  // -------------------- Generate cube -------------------- //
  std::cout << "Generate cube" << std::endl;
  generate_cube(size_x, size_y, size_k, cube_file_name);
  std::cout << "done" << std::endl;


  // -------------------- Dump median values -------------------- //
  if (!median_value_file_name.empty()) {
    std::cout << "\nDump median values" << std::endl;
    dump_median_value(size_x, size_y, size_k, cube_file_name, median_value_file_name);
    std::cout << "done" << std::endl;
  }

  return 0;
}