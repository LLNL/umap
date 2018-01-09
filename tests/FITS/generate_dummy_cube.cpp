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
#include <algorithm>
#include <tuple>


#ifdef _OPENMP

#include <omp.h>


#endif

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
            << "-m: file name to dump median values"
            << "-s: if specified, split cube file by frame" << std::endl;
}

void parse_option(const int argc, char *const argv[],
                  size_t &size_x, size_t &size_y, size_t &size_k,
                  std::string &cube_file_name, std::string &median_value_file_name,
                  bool &split_by_frame)
{
  bool option_x_found = false;
  bool option_y_found = false;
  bool option_k_found = false;
  bool option_c_found = false;

  int p;
  while ((p = ::getopt(argc, argv, "x:y:k:c:m:sh")) != -1) {
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
        break;

      case 's':
        split_by_frame = true;
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
// Create and mapfile
// -------------------------------------------------------------------------------- //
std::pair<int, void *>
create_and_map_file(const std::string &file_name, void *const base_address, const size_t size)
{
  int fd;
  void *map;
  if (base_address != nullptr)
    std::tie(fd, map) = utility::create_and_map_file_write_mode(file_name, base_address, size, 0, MAP_FIXED);
  else
    std::tie(fd, map) = utility::create_and_map_file_write_mode(file_name, base_address, size, 0);

  if (fd == -1 || map == nullptr) {
    std::cerr << "Failed to map file: " << file_name << std::endl;
    std::exit(1);
  }

  return std::make_pair(fd, map);
}


// -------------------------------------------------------------------------------- //
// Create and map cube file
// -------------------------------------------------------------------------------- //
std::vector<std::pair<int, void *>>
create_and_map_cube_file(const size_t size_x, const size_t size_y, const size_t size_k,
                         const std::string &cube_file_name, const bool split_by_frame)
{
  std::vector<std::pair<int, void *>> map_list;
  if (split_by_frame) {
    map_list.resize(size_k);

    const size_t cube_size = size_x * size_y * size_k * sizeof(value_type);
    void *base_address = utility::reserve_vm_address(cube_size);
    utility::mprotect_write(base_address, cube_size);

    for (size_t k = 0; k < size_k; ++k) {
      const std::string file_name(cube_file_name + std::to_string(k + 1) + ".data");
      const size_t frame_size = size_x * size_y * sizeof(value_type);

      map_list[k] = create_and_map_file(file_name, base_address, frame_size);
      if (base_address != map_list[k].second) {
        std::cerr << base_address << " != " << map_list[k].second << std::endl;
        std::exit(1);
      }

      base_address = reinterpret_cast<char *>(base_address) + frame_size;
    }
  } else {
    const size_t cube_size = size_x * size_y * size_k * sizeof(value_type);
    map_list.emplace_back(create_and_map_file(cube_file_name, nullptr, cube_size));
  }

  return map_list;
}

// -------------------------------------------------------------------------------- //
// Generate cube
// -------------------------------------------------------------------------------- //
void generate_cube(const size_t size_x, const size_t size_y, const size_t size_k, value_type *const cube)
{
  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
  std::mt19937_64 gen(seed);
  std::uniform_real_distribution<value_type> dis(0.0, 8192.0); //TODO: check reasonable value range

#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (size_t k = 0; k < size_k; ++k) {
    for (size_t y = 0; y < size_y; ++y) {
      for (size_t x = 0; x < size_x; ++x) {
        const value_type value = median::reverse_byte_order(dis(gen));
        cube[x + size_x * y + size_x * size_y * k] = value;
      }
    }
  }

}


// -------------------------------------------------------------------------------- //
// Calculate median value
// -------------------------------------------------------------------------------- //
std::vector<value_type>
calculate_median_value(const size_t size_x, const size_t size_y, const size_t size_k, const value_type *const cube)
{

  std::vector<value_type> median_value;
  median_value.resize(size_x * size_y);

#ifdef _OPENMP
#pragma omp parallel
#endif
  {
    std::vector<value_type> work;
    work.resize(size_k);

    // ----- Find median value ----- //
#ifdef _OPENMP
#pragma omp for
#endif
    for (size_t y = 0; y < size_y; ++y) {
      for (size_t x = 0; x < size_x; ++x) {
        for (size_t k = 0; k < size_k; ++k) {
          work[k] = median::reverse_byte_order(cube[x + size_x * y + size_x * size_y * k]);
        }
        std::sort(work.begin(), work.end());

        if (size_k % 2 == 0) {
          median_value[y * size_x + x] = static_cast<float>((work[size_k / 2 - 1] + work[size_k / 2]) / 2.0);
        } else {
          median_value[y * size_x + x] = work[size_k / 2];
        }
      } // x axis
    } // y axis, omp for

  } // omp parallel

  return median_value;
}

// -------------------------------------------------------------------------------- //
// Dump median value
// -------------------------------------------------------------------------------- //
void dump_median_value(const std::vector<value_type> median_value, const std::string &median_file_name)
{
  std::ofstream of(median_file_name);
  if (!of.is_open()) {
    std::cerr << "Can not open: " << median_file_name << std::endl;
    std::exit(1);
  }

  for (auto value : median_value) {
    of << value << "\n";
  }

  of.close();
}


int main(int argc, char *argv[])
{
  // -------------------- Initialization -------------------- //
  size_t size_x; // Width of a frame
  size_t size_y; // Height of a frame
  size_t size_k; // #frames
  std::string cube_file_name;
  std::string median_value_file_name;
  bool split_by_frame = false; // if true, split cube file by frame

  parse_option(argc, argv, size_x, size_y, size_k, cube_file_name, median_value_file_name, split_by_frame);

  const size_t cube_size = size_x * size_y * size_k * sizeof(value_type);
  std::cout << "size (GB) = " << cube_size / static_cast<double>((1ULL << 30)) << std::endl;

  // -------------------- Create and map cube -------------------- //
  std::cout << "\nCreate and map cube" << std::endl;
  auto map_list = create_and_map_cube_file(size_x, size_y, size_k, cube_file_name, split_by_frame);
  value_type *cube = reinterpret_cast<value_type *>(map_list[0].second);
  std::cout << "done" << std::endl;

  // -------------------- Generate cube -------------------- //
  std::cout << "\nGenerate cube" << std::endl;
  generate_cube(size_x, size_y, size_k, cube);
  std::cout << "done" << std::endl;

  // -------------------- Dump median values -------------------- //
  if (!median_value_file_name.empty()) {
    std::cout << "\nCalculate median values" << std::endl;
    auto median_value = calculate_median_value(size_x, size_y, size_k, cube);
    std::cout << "done" << std::endl;

    std::cout << "\nDump median values" << std::endl;
    dump_median_value(median_value, median_value_file_name);
    std::cout << "done" << std::endl;
  }

  // -------------------- Unmap cube -------------------- //
  if (split_by_frame) {
    const size_t frame_size = size_x * size_y * sizeof(value_type);
    for (size_t k = 0; k < size_k; ++k) {
      utility::munmap(map_list[k].first, map_list[k].second, frame_size);
    }
  } else {
    utility::munmap(map_list[0].first, map_list[0].second, cube_size);
  }

  return 0;
}