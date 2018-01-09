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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <unistd.h>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "umap.h"
#include "umaptest.h"
#include "median_calculation_kernel.hpp"
#include "../utility/time.hpp"

using pixcel_value_type = float;

void dump_median_value(const std::vector<pixcel_value_type> &median_value, const std::string& result_file_name)
{
  std::ofstream of(result_file_name);
  if (!of.is_open()) {
    std::cerr << "Can not open: " << result_file_name << std::endl;
    std::exit(-1);
  }

  for (auto value : median_value) {
    of << value << "\n";
  }

  of.close();
}

int main(int argc, char *argv[])
{
  // -------------------- Initialization -------------------- //
  median::cube_t<pixcel_value_type> cube;
  cube.size_x = 1024;
  cube.size_y = 1024;
  cube.size_k = 10;
  std::string median_result_file_name; // if you don't need to check results (median value), empty this string object.

  // Parse general options
  umt_optstruct_t options;
  umt_getoptions(&options, argc, argv);

  const size_t frame_mem_size = median::get_frame_size(cube) * sizeof(pixcel_value_type);
  const size_t cube_mem_size = median::get_cube_size(cube) * sizeof(pixcel_value_type);
  omp_set_num_threads(static_cast<int>(options.numthreads));

  // -------------------- Map the file -------------------- //
  void* raw_pointer;
  std::cout << "Map files" << std::endl;
  void* bk_list = umt_openandmap_mf(&options, cube_mem_size, &raw_pointer, 0, frame_mem_size);
  assert(bk_list != nullptr);
  cube.data = reinterpret_cast<pixcel_value_type *>(raw_pointer);
  std::cout << "done" << std::endl;

  // -------------------- Calculate median -------------------- //
  std::vector<pixcel_value_type> median_calculation_result(median::get_frame_size(cube));
  std::cout << "An array is allocated to store median values (GB) "
            << static_cast<double>(frame_mem_size) / (1ULL << 30) << std::endl;

  std::cout << "\nMedian calculation start" << std::endl;
  const auto time_start = utility::elapsed_time_sec();
  median::calculate_median<pixcel_value_type>(cube, median_calculation_result.data());
  std::cout << "done (sec) " << utility::elapsed_time_sec(time_start) << std::endl;

  if (!median_result_file_name.empty()) {
    std::cout << "\nDump median value" << std::endl;
    dump_median_value(median_calculation_result, median_result_file_name);
    std::cout << "done" << std::endl;
  }

  umt_closeandunmap_mf(&options, cube_mem_size, raw_pointer, bk_list);

  return 0;
}