#include <iostream>
#include <iomanip>
#include <cassert>
#include <byteswap.h>
#include "testoptions.h"
#include "PerFits.h"

#define HEX( x ) std::setw(2) << std::setfill('0') << std::hex << (int)( x )

static void swapbyte(float *a, float *b)
{
  *(uint32_t*)b = bswap_32(*(uint32_t*)(a));
}

float pixel_value(float* cube, size_t xDim, size_t yDim, size_t x, size_t y, size_t z)
{
  // FITS arrays are column major, just like FORTRAN
  float rval;
  float* addr = cube + ( ( z * ( xDim * yDim ) ) + ( x * yDim ) + y );

  swapbyte(addr, &rval); // Row major order

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

  size_t x = 1058;
  size_t y = 124;
  size_t z = 0;

  x = 1058; y = 124; z = 0;
  std::cout << "epoch(z) " << std::dec << z << " x_pixel_loc " << x << " y_pixel_loc " << y << " pixel_value " << pixel_value(mycube, xDim, yDim, x, y, z) << std::endl;

  x = 64; y = 996; z = 1;
  std::cout << "epoch(z) " << std::dec << z << " x_pixel_loc " << x << " y_pixel_loc " << y << " pixel_value " << pixel_value(mycube, xDim, yDim, x, y, z) << std::endl;

  x = 512; y = 3; z = 5;
  std::cout << "epoch(z) " << std::dec << z << " x_pixel_loc " << x << " y_pixel_loc " << y << " pixel_value " << pixel_value(mycube, xDim, yDim, x, y, z) << std::endl;

  PerFits::PerFits_free_cube(mycube);
  return 0;
}
