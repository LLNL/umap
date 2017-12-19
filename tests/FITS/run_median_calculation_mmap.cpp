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
#include <unistd.h>
#include <tuple>

#include "../utility/time.hpp"
#include "../utility/openmp.hpp"
#include "../utility/mmap.hpp"

#include "median_calculation_kernel.hpp"

using value_type = float;


// -------------------------------------------------------------------------------- //
// Parse command line options
// -------------------------------------------------------------------------------- //
void usage()
{
  std::cout << "-x: size of width of a frame (required)\n"
            << "-y: size of height of a frame (required)\n"
            << "-k: number of frames (required)\n"
            << "-c: file name of the cube file (required)\n"
            << "-r: file name to dump the median values" << std::endl;
}

void parse_option(const int argc, char *const argv[],
                  size_t &size_x, size_t &size_y, size_t &size_k,
                  std::string &cube_file_name, std::string &result_file_name)
{
  bool option_x_found = false;
  bool option_y_found = false;
  bool option_k_found = false;
  bool option_c_found = false;

  int p;
  while ((p = ::getopt(argc, argv, "x:y:k:c:r:h")) != -1) {
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

      case 'r':
        result_file_name = optarg;
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
// Map file
// -------------------------------------------------------------------------------- //
std::pair<int, value_type*> map_file(const std::string& cube_file_name, const size_t cube_size)
{
  auto ret = utility::map_file_read_mode(cube_file_name, nullptr, cube_size, 0);
  const int fd = ret.first;
  value_type *const cube = reinterpret_cast<value_type *>(ret.second);
  if (fd == -1 || cube == nullptr) {
    std::cerr << "Failed to map file: " << cube_file_name << std::endl;
    std::exit(1);
  }

  return std::make_pair(fd, cube);
}

void munmap(const int fd, value_type *const cube, const size_t cube_size)
{

  utility::munmap(fd, cube, cube_size);
}


// -------------------------------------------------------------------------------- //
// Dump result
// -------------------------------------------------------------------------------- //
void dump_result(const size_t size_x, const size_t size_y,
                 const value_type *const median_calculation_result,
                 const std::string &result_file_name)
{
  std::ofstream of(result_file_name);
  if (!of.is_open()) {
    std::cerr << "Can not open: " << result_file_name << std::endl;
    std::exit(1);
  }

  for (size_t y = 0; y < size_y; ++y) {
    for (size_t x = 0; x < size_x; ++x) {
      of << median_calculation_result[x + y * size_x] << "\n";
    }
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
  std::string result_file_name;

  parse_option(argc, argv, size_x, size_y, size_k, cube_file_name, result_file_name);

  const size_t cube_size = size_x * size_y * size_k * sizeof(value_type);


  // -------------------- Map the file -------------------- //
  int fd;
  value_type *cube;
  std::tie(fd, cube) = map_file(cube_file_name, cube_size);


  // -------------------- Calculate median -------------------- //
  const size_t flame_size = size_x * size_y;
  value_type *median_calculation_result = new value_type[flame_size];
  std::cout << "An array is allocated to store median values (GB) "
            << static_cast<double>(flame_size * sizeof(value_type)) / (1ULL << 30) << std::endl;

  std::cout << "\nMedian calculation start" << std::endl;
  const auto time_start = utility::elapsed_time_sec();
  median::calculate_median<value_type>(cube, size_x, size_y, size_k, median_calculation_result);
  std::cout << "done (sec) " << utility::elapsed_time_sec(time_start) << std::endl;


  // -------------------- Dump the result -------------------- //
  if (!result_file_name.empty()) {
    std::cout << "\nDump result" << std::endl;
    dump_result(size_x, size_y, median_calculation_result, result_file_name);
    std::cout << "done" << std::endl;
  }


  // -------------------- Closing -------------------- //
  munmap(fd, cube, cube_size);
  delete[] median_calculation_result;

  return 0;
}
