//
// Created by Iwabuchi, Keita on 4/5/18.
//

#include "median_calculation_kernel.hpp"
#include "testoptions.h"
#include "PerFits.h"

int main(int argc, char** argv)
{
  umt_optstruct_t options;
  umt_getoptions(&options, argc, argv);

  size_t BytesPerElement;
  size_t xDim;
  size_t yDim;
  size_t zDim;
  //float *cube_pointer = new float[4096 * 4096 * 10]; // just a dummy memory region for test
  median::cube_t<float> cube;

  cube.data = (float*)PerFits::PerFits_alloc_cube(&options, &BytesPerElement, &xDim, &yDim, &zDim);

  cube.size_x = xDim;
  cube.size_y = yDim;
  cube.size_k = zDim; // tne number of frames

  median::vector_t vector;
  vector.x_intercept = 10;
  vector.x_slope = 3.5;
  vector.y_intercept = 20.0;
  vector.y_slope = 5.0;

  float median_value = median::torben(cube, vector);
  PerFits::PerFits_free_cube(cube.data);
}
