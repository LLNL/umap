#include <iostream>
#include "testoptions.h"
#include "PerFits.h"

static void swapbyte(float *a, float *b)
{
  unsigned char *a1=(unsigned char *)a;
  unsigned char *b1=(unsigned char *)b;
  b1[0] = a1[3];
  b1[1] = a1[2];
  b1[2] = a1[1];
  b1[3] = a1[0];
}

float pixel_value(float* cube, size_t xDim, size_t yDim, size_t x, size_t y, size_t z)
{
  float rval;

  swapbyte(cube + ( ( z * ( xDim * yDim ) ) + ( y * xDim ) + x ), &rval); // Row major order

  return rval;
}

int main(int argc, char * argv[])
{
  umt_optstruct_t options;
  umt_getoptions(&options, argc, argv);
  float* mycube;
  size_t BytesPerElement;
  size_t xDim;
  size_t yDim;
  size_t zDim;

  mycube = (float*)PerFits::PerFits_alloc_cube(&options, &BytesPerElement, &xDim, &yDim, &zDim);
  size_t z = 0;
  size_t y = 124;
  size_t x = 1058;

  std::cout << "epoch(z) " << z << " x_pixel_loc " << x << " y_pixel_loc " << y << " pixel_value " << pixel_value(mycube, xDim, yDim, 1058, 124, 0) << std::endl;
  PerFits::PerFits_free_cube(mycube);
  return 0;
}
