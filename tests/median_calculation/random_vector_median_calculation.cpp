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
#include <random>
#include <vector>
#include <algorithm>

#include "median_calculation_kernel.hpp"
#include "testoptions.h"
#include "PerFits.h"

using pixel_type = float;

class beta_distribution
{
 public:
  beta_distribution(double a, double b)
      : m_x_gamma(a, 1.0),
        m_y_gamma(b, 1.0)
  { }

  template <typename rnd_engine>
  double operator()(rnd_engine& engine) {
    double x = m_x_gamma(engine);
    double y = m_y_gamma(engine);
    return x / (x + y);
  }

 private:
  std::gamma_distribution<> m_x_gamma;
  std::gamma_distribution<> m_y_gamma;
};

int main(int argc, char** argv)
{
  umt_optstruct_t options;
  umt_getoptions(&options, argc, argv);

  size_t BytesPerElement;
  size_t xDim;
  size_t yDim;
  size_t zDim;
  median::cube_t<pixel_type> cube;

  cube.data = (pixel_type*)PerFits::PerFits_alloc_cube(&options, &BytesPerElement, &xDim, &yDim, &zDim);

  cube.size_x = xDim;
  cube.size_y = yDim;
  cube.size_k = zDim; // tne number of frames

  std::mt19937 rnd_engine(123);
  std::uniform_int_distribution<int> x_start_dist(0.2 * cube.size_x, 0.8 * cube.size_x);
  std::uniform_int_distribution<int> y_start_dist(0.2 * cube.size_y, 0.8 * cube.size_y);
  beta_distribution x_beta_dist(3, 2);
  beta_distribution y_beta_dist(3, 2);
  std::discrete_distribution<int> plus_or_minus{-1, 1};

  using median_result_type = std::pair<pixel_type, std::vector<median::read_value_t<pixel_type>>>;
  std::vector<median_result_type> results;
  for (int i = 0; i < 10000; ++i) {
    double x_intercept = x_start_dist(rnd_engine);
    double y_intercept = y_start_dist(rnd_engine);

    double x_slope = x_beta_dist(rnd_engine) * plus_or_minus(rnd_engine) * 25;
    double y_slope = y_beta_dist(rnd_engine) * plus_or_minus(rnd_engine) * 25;

    auto ret = median::torben(cube, {x_intercept, x_slope, y_intercept, y_slope});
    results.emplace_back(std::move(ret));
  }

  /// Sort the results by the descending order of median value
  std::sort(results.begin(), results.end(), [](const median_result_type& lhd, const median_result_type& rhd) {
    return (lhd.first > rhd.first);
  });

  for (size_t i = 0; i < 10; ++i) {
    const auto elem = results[i];
    std::cout << elem.first << std::endl;
    for (auto read_value : elem.second) {
      std::cout << read_value.pos_x << "\t" << read_value.pos_y << "\t" << read_value.value << std::endl;
    }
    std::cout << std::endl;
  }

  PerFits::PerFits_free_cube(cube.data);
}
