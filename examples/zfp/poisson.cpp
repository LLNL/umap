// Solve Poisson equation div(grad(u(x, y, z))) = sqrt(x^2 + y^2 + z^2)

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <chrono>

#if POISSON_WITH_ZFP
#include "zfp/array3.hpp"
#include "zfp/codec/gencodec.hpp"
#ifdef USE_UMAP
#include "zfp/array3umap.hpp"
#endif
#endif

#include "array.hpp"
#include "laplace.hpp"
#include "solver.hpp"

// include support for half precision?
#ifdef __GNUC__
  #define POISSON_WITH_HALF 1
#endif

// include support for posits?
#if POISSON_WITH_POSITS
  #include "numrep.hpp"
  typedef NumRep<unsigned short, ExpGolombRice<2>, MapLinear> Posit16;
  typedef NumRep<unsigned int,   ExpGolombRice<2>, MapLinear> Posit32;
  typedef NumRep<unsigned long,  ExpGolombRice<2>, MapLinear> Posit64;
#endif

// include support for multi-posits?
#if POISSON_WITH_MP
  #include "unicodec.h"
#endif

// available number representations
enum Type {
#if POISSON_WITH_HALF
  typeHalf,
#endif
  typeFloat,
  typeDouble,
#if POISSOn_WITH_POSITS
  typePosit16,
  typePosit32,
  typePosit64,
#endif
  typeZFP
};

// run Poisson solver with templated array type
template <class array>
int
execute(array& u, int order, size_t iterations, const char* upath, const char* ddupath)
{
  // run iterative solver and optionally output solution and/or Laplacian
  std::chrono::time_point<std::chrono::steady_clock> timing_st = std::chrono::steady_clock::now();
  switch (order) {
   case 2: { PoissonSolver<array, 2> solver(u); solver.init(); solver.solve(iterations); solver.output(upath, ddupath); } break;
   case 4: { PoissonSolver<array, 4> solver(u); solver.init(); solver.solve(iterations); solver.output(upath, ddupath); } break;
   case 6: { PoissonSolver<array, 6> solver(u); solver.init(); solver.solve(iterations); solver.output(upath, ddupath); } break;
   case 8: { PoissonSolver<array, 8> solver(u); solver.init(); solver.solve(iterations); solver.output(upath, ddupath); } break;
   default:
     fprintf(stderr, "unsupported order of accuracy\n");
     return EXIT_FAILURE;
  }
  std::chrono::time_point<std::chrono::steady_clock> timing_end = std::chrono::steady_clock::now();
  fprintf(stderr, "%zu ms\n",
	  (size_t)std::chrono::duration_cast<std::chrono::milliseconds>(timing_end - timing_st).count());

  return EXIT_SUCCESS;
}

// use uncompressed linear array
template <typename Real>
int
execute_linear_array(size_t nx, size_t ny, size_t nz, int order, size_t iterations, const char* upath, const char* ddupath)
{
  array3<Real> u(nx, ny, nz);
  return execute(u, order, iterations, upath, ddupath);
}

#if POISSON_WITH_ZFP
// use uncompressed tiled array
template <typename Real>
int
execute_tiled_array(size_t nx, size_t ny, size_t nz, int order, size_t iterations, const char* upath, const char* ddupath, size_t cache_size = 0)
{
  if (!cache_size)
    cache_size = nx * ny * 8 * sizeof(double);
  zfp::array3< double, zfp::codec::generic3<double, Real> > u(nx, ny, nz, sizeof(Real) * CHAR_BIT, 0, cache_size);
  return execute(u, order, iterations, upath, ddupath);
}

// use compressed zfp array
int
execute_zfp_array(size_t nx, size_t ny, size_t nz, int order, size_t iterations, const char* upath, const char* ddupath, double rate, size_t cache_size = 0, bool use_umap=false)
{
#ifdef USE_UMAP
  if( !use_umap ){
#endif
    if (!cache_size)
      cache_size = nx * ny * 8 * sizeof(double);
    zfp::array3d u(nx, ny, nz, rate, 0, cache_size);
    return execute(u, order, iterations, upath, ddupath);
#ifdef USE_UMAP
  }else{
    zfp::array3dumap u(nx, ny, nz, rate);
    return execute(u, order, iterations, upath, ddupath);
  }
#endif
}
#else
// stub for silencing compiler (should never arrive here)
template <typename Real>
int
execute_tiled_array(size_t, size_t, size_t, int, size_t, const char*, const char*, size_t = 0)
{
  return EXIT_FAILURE;
}
#endif

int usage()
{
  fprintf(stderr, "Usage: poisson [options]\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -n <nx> <ny> <nz> : grid dimensions (without ghost cells)\n");
  fprintf(stderr, "  -t <iterations> : number of iterations\n");
  fprintf(stderr, "  -p <order> : order of accuracy (2, 4, 6, or 8)\n");
#if POISSON_WITH_HALF
  fprintf(stderr, "  -h : half precision\n");
#endif
  fprintf(stderr, "  -f : single precision\n");
  fprintf(stderr, "  -d : double precision\n");
#if POISSON_WITH_ZFP
  fprintf(stderr, "  -r <rate> [-u]: zfp fixed rate [use Umap backend]\n");
  fprintf(stderr, "  -z : use zfp tiled arrays with scalar storage\n");
  fprintf(stderr, "  -c <size> : cache size in bytes\n");
#endif
  fprintf(stderr, "  -o <path> : output solution to file\n");
  fprintf(stderr, "  -l <path> : output Laplacian to file\n");
  return EXIT_FAILURE;
}

int main(int argc, char* argv[])
{
  const size_t ng = GHOST_LAYERS; // number of ghost layers
  Type type = typeDouble; // number representation
  size_t mx = 512; // number of grid cells per dimension
  size_t my = 512; // number of grid cells per dimension
  size_t mz = 512; // number of grid cells per dimension
  size_t cache_size = 0; // cache size in bytes
  int order = 2; // differential operator order of accuracy
  bool tiled_array = false; // use zfp tiled array
  size_t iterations = 3; //10000; // number of iterations
  char* upath = 0; // path to optional output file
  char* ddupath = 0; // path to optional output file
#if POISSON_WITH_ZFP
  double rate = 16; // zfp rate in bits/value
  bool use_umap = false; // by default, do not use umap backend
#endif

  // process command-line arguments
  for (int i = 1; i < argc; i++)
    if (std::string(argv[i]) == "-n") {
      if (++i == argc || sscanf(argv[i], "%zu", &mx) != 1 ||
          ++i == argc || sscanf(argv[i], "%zu", &my) != 1 ||
          ++i == argc || sscanf(argv[i], "%zu", &mz) != 1)
        return usage();
    }
    else if (std::string(argv[i]) == "-t") {
      if (++i == argc || sscanf(argv[i], "%zu", &iterations) != 1)
        return usage();
    }
    else if (std::string(argv[i]) == "-p") {
      if (++i == argc || sscanf(argv[i], "%d", &order) != 1)
        return usage();
    }
#if POISSON_WITH_HALF
    else if (std::string(argv[i]) == "-h")
      type = typeHalf;
#endif
    else if (std::string(argv[i]) == "-f")
      type = typeFloat;
    else if (std::string(argv[i]) == "-d")
      type = typeDouble;
#if POISSON_WITH_ZFP
    else if (std::string(argv[i]) == "-r") {
      type = typeZFP;
      if (++i == argc || sscanf(argv[i], "%lf", &rate) != 1)
        return usage();
      if ( (i+1) < argc && std::string(argv[i+1]) == "-u"){
        use_umap = true;
	++i;
      }

    }
    else if (std::string(argv[i]) == "-z")
      tiled_array = true;
    else if (std::string(argv[i]) == "-c") {
      if (++i == argc || sscanf(argv[i], "%zu", &cache_size) != 1)
        return usage();
    }
#endif
    else if (std::string(argv[i]) == "-o") {
      if (++i == argc)
        return usage();
      upath = argv[i];
    }
    else if (std::string(argv[i]) == "-l") {
      if (++i == argc)
        return usage();
      ddupath = argv[i];
    }
    else
      return usage();

  // array dimensions with ghost layers
  const size_t nx = ng + mx + ng;
  const size_t ny = ng + my + ng;
  const size_t nz = ng + mz + ng;

  // launch Poisson solver
  switch (type) {
#if POISSON_WITH_HALF
    case typeHalf:
      if (tiled_array)
        return execute_tiled_array<__fp16>(nx, ny, nz, order, iterations, upath, ddupath, cache_size);
      else
        return execute_linear_array<__fp16>(nx, ny, nz, order, iterations, upath, ddupath);
#endif
    case typeFloat:
      if (tiled_array)
        return execute_tiled_array<float>(nx, ny, nz, order, iterations, upath, ddupath, cache_size);
      else
        return execute_linear_array<float>(nx, ny, nz, order, iterations, upath, ddupath);
    case typeDouble:
      if (tiled_array)
        return execute_tiled_array<double>(nx, ny, nz, order, iterations, upath, ddupath, cache_size);
      else
        return execute_linear_array<double>(nx, ny, nz, order, iterations, upath, ddupath);
#if POISSON_WITH_POSITS
    case typePosit16:
      if (tiled_array)
        return execute_tiled_array<Posit16>(nx, ny, nz, order, iterations, upath, ddupath, cache_size);
      else
        return execute_linear_array<Posit16>(nx, ny, nz, order, iterations, upath, ddupath);
    case typePosit32:
      if (tiled_array)
        return execute_tiled_array<Posit32>(nx, ny, nz, order, iterations, upath, ddupath, cache_size);
      else
        return execute_linear_array<Posit32>(nx, ny, nz, order, iterations, upath, ddupath);
    case typePosit64:
      if (tiled_array)
        return execute_tiled_array<Posit64>(nx, ny, nz, order, iterations, upath, ddupath, cache_size);
      else
        return execute_linear_array<Posit64>(nx, ny, nz, order, iterations, upath, ddupath);
#endif
#if POISSON_WITH_ZFP
    case typeZFP:
      return execute_zfp_array(nx, ny, nz, order, iterations, upath, ddupath, rate, cache_size, use_umap);
#endif
    default:
      return EXIT_FAILURE;
  }
}
