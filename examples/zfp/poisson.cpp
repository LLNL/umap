// Solve Poisson equation div(grad(u)) = f(x, y, z) = sqrt(x^2 + y^2 + z^2)

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <chrono>

#include "zfp.h"
#include "array3.hpp"
#include "array3umap.hpp"
#include <sstream>
#include <iostream>

// use Jacobi (when defined) or Gauss-Seidel update (set via Makefile)
#define JACOBI 1

// finite-difference order (set via Makefile)
#define ORDER 2
//#define ORDER 4
//#define ORDER 6
//#define ORDER 8

#define ZFP_RATE 16
#define USE_UMAP 1

// number representation to use (set via Makefile)
#if defined(IEEE_PREC) // IEEE-754 precision in bits/value when defined
  #if IEEE_PREC == 16
    #define REAL __fp16 // IEEE half precision
  #elif IEEE_PREC == 32
    #define REAL float // IEEE single precision
  #elif IEEE_PREC == 64
    #define REAL double // IEEE double precision
  #else
    #error "unsupported IEEE_PREC"
  #endif
#elif defined(POSIT_PREC) // posit precision in bits/value when defined
  #include "numrep.hpp"
  #if POSIT_PREC == 16
    #define REAL NumRep<unsigned short, ExpGolombRice<2>, MapLinear>
  #elif POSIT_PREC == 32
    #define REAL NumRep<unsigned int, ExpGolombRice<2>, MapLinear>
  #elif POSIT_PREC == 64
    #define REAL NumRep<unsigned long, ExpGolombRice<2>, MapLinear>
  #else
    #error "unsupported POSIT_PREC"
  #endif
#elif defined(ZFP_RATE) // zfp rate in bits/value when defined
  #include "array3.hpp"
#elif defined(ZFP_TOLERANCE) // zfp absolute error tolerance when defined
  #include "zfpvarray3.h"
#else
  #error "no number representation specified"
#endif

// storage mask for querying array payload
#ifdef ZFP_DATA_PAYLOAD
  #define PAYLOAD ZFP_DATA_PAYLOAD
#else
  #define PAYLOAD 0
#endif

    


    
// minimal 3D array class templated on scalar type
template <typename real>
class array3 {
public:
  // constructor
  array3(size_t nx, size_t ny, size_t nz) :
    nx(nx), ny(ny), nz(nz),
    data(new real[nx * ny * nz])
  {
    std::fill(data, data + nx * ny * nz, real(0));
  }

  // destructor
  ~array3() {
#if IEEE_PREC == 64    
    FILE * pFile;
    std::stringstream ss;
    ss <<"poisson_double_"<<nx<<"x"<<ny<<"x"<<nz<<".vtk";
    pFile = fopen (ss.str().c_str(), "w");
    fprintf (pFile, "# vtk DataFile Version 2.0\nVolume example\nASCII\nDATASET STRUCTURED_POINTS\n");
    fprintf (pFile, "DIMENSIONS %zu %zu %zu\n", nx, ny, nz);
    fprintf (pFile, "ASPECT_RATIO 1 1 1\n");
    fprintf (pFile, "ORIGIN 0 0 0\n");
    fprintf (pFile, "POINT_DATA %zu\n", nx*ny*nz);
    fprintf (pFile, "SCALARS volume_scalars float 1\nLOOKUP_TABLE default\n");
    for (size_t k = 0; k < nz; k++) {
      for (size_t j = 0; j < ny; j++) {
        for (size_t i = 0; i < nx; i++) {
          fprintf(pFile, "%.4f ", data[i + nx * (j + ny * k)] );
        }
        fprintf(pFile, "\n");
      }
    }
    fclose (pFile);

    /* save uncompressed array */
    ss.str("");
    ss <<"poisson_double_"<<nx<<"x"<<ny<<"x"<<nz<<"_uncompressed.bin";
    pFile = fopen (ss.str().c_str(), "wb");
    fwrite (data , 1, sizeof(real)*nx * ny * nz, pFile);
    fclose (pFile);

    /* compressed stream */
    zfp_stream* zfp = zfp_stream_open(NULL);

    zfp_type type = zfp_type_double;
    double rate = 16.0;
    zfp_field* field = zfp_field_3d(data, type, nx, ny, nz);
    /* use fixed rate */
    zfp_stream_set_rate(zfp, rate, type, zfp_field_dimensionality(field), zfp_true);

    /* allocate buffer for compressed data */
    size_t compressed_bufsize = zfp_stream_maximum_size(zfp, field);
    void* compressed_buffer = malloc(compressed_bufsize);

    /* bit stream to write to or read from */
    bitstream* stream = stream_open(compressed_buffer, compressed_bufsize);

    /* Associate compressed stream with bit stream for reading and writing bits to/from memory. */
    zfp_stream_set_bit_stream(zfp, stream);

    /* Compress the whole array */
    zfp_stream_rewind(zfp);
    size_t zfpsize = zfp_compress(zfp, field);
    if (!zfpsize) {
      fprintf(stderr, "compression failed\n");
      exit(13);
    }
    size_t array_bytes = zfp_field_size(field, NULL) * sizeof(double); 
    printf("\ncompressed %zu bytes to %zu bytes (%.2f%%)\n", array_bytes, zfpsize, zfpsize*100.0/array_bytes);

    ss.str("");
    ss <<"poisson_double_"<<nx<<"x"<<ny<<"x"<<nz<<"_compressed_rate"<<int(rate)<<".bin";
    pFile = fopen (ss.str().c_str(), "wb");
    fwrite (compressed_buffer , 1, compressed_bufsize, pFile);
    fclose (pFile);

    /* Free up */
    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);
    free(compressed_buffer);
#endif

    delete data; 
  }

  // copy constructor (deep copy)
  array3(const array3& that) { copy(that); }

  // copy operator (deep copy)
  array3& operator=(const array3& that)
  {
    if (this != &that)
      copy(that);
    return *this;
  }

  // array dimensions
  size_t size() const { return nx * ny * nz; }
  size_t size_x() const { return nx; }
  size_t size_y() const { return ny; }
  size_t size_z() const { return nz; }

  // storage size
  size_t size_bytes(unsigned int = 0) const { return size() * sizeof(real); }

  // accessors
  real& operator()(size_t x, size_t y, size_t z) { return data[x + nx * (y + ny * z)]; }
  const real& operator()(size_t x, size_t y, size_t z) const { return data[x + nx * (y + ny * z)]; }

  // write to file
  bool write(const char* path) const
  {
    FILE* file = fopen(path, "wb");
    if (!file)
      return false;
    bool success = (fwrite(data, sizeof(*data), size(), file) == size());
    fclose(file);
    return success;
  }

protected:
  // deep copy
  void copy(const array3& that)
  {
    nx = that.nx;
    ny = that.ny;
    nz = that.nz;
    if (data)
      delete[] data;
    data = new real[nx * ny * nz];
    std::copy(that.data, that.data + nx * ny * nz, data);
  }

  size_t nx, ny, nz; // array dimensions
  real* data;        // array contents
};

// initialize u(x, y, z) = r^3 / 12 on boundary only
template <class array>
static void
init(array& u)
{
  const size_t p = ORDER / 2;
  const size_t nx = u.size_x();
  const size_t ny = u.size_y();
  const size_t nz = u.size_z();
  const double dx = 2. / nx;
  const double dy = 2. / ny;
  const double dz = 2. / nz;

  for (size_t k = 0; k < nz; k++) {
    double z = dz * (k - 0.5 * (nz - 1));
    for (size_t j = 0; j < ny; j++) {
      double y = dy * (j - 0.5 * (ny - 1));
      for (size_t i = 0; i < nx; i++) {
        double x = dx * (i - 0.5 * (nx - 1));
        double r = std::sqrt(x * x + y * y + z * z);
        double val = (i < p || i >= nx - p || j < p || j >= ny - p || k < p || k >= nz - p) ? r * r * r / 12 : 0.0;
        u(i, j, k) = val;
      }
    }
  }
}

// solve Poisson equation using Jacobi or Gauss-Seidel iteration
template <class array>
static void
iterate(array& u, size_t iterations, const char* path = 0)
{
  // 1D finite-difference stencils for various orders of accuracy
  #if ORDER == 2
    const double fdnum[] = { -2, 1 };
    const double fdden = 1;
  #elif ORDER == 4
    const double fdnum[] = { -30, 16, -1 };
    const double fdden = 12;
  #elif ORDER == 6
    const double fdnum[] = { -490, 270, -27, 2 };
    const double fdden = 180;
  #elif ORDER == 8
    const double fdnum[] = { -14350, 8064, -1008, 128, -9 };
    const double fdden = 5040;
  #else
    #error "ORDER must be one of {2, 4, 6, 8}"
  #endif

  // precompute constants to accelerate computation
  const size_t p = ORDER / 2;

  // grid dimensions
  const size_t nx = u.size_x();
  const size_t ny = u.size_y();
  const size_t nz = u.size_z();
  const size_t n = (nx - 2 * p) * (ny - 2 * p) * (nz - 2 * p);

  // grid spacing
  const double dx = 2. / nx;
  const double dy = 2. / ny;
  const double dz = 2. / nz;

  // unnormalized inner-product weights
  const double vx = 1. / (dx * dx);
  const double vy = 1. / (dy * dy);
  const double vz = 1. / (dz * dz);
  const double vr = -fdden;
  const double denom = -fdnum[0] * (vx + vy + vz);

  // normalized inner-product weights
  const double wx = vx / denom;
  const double wy = vy / denom;
  const double wz = vz / denom;
  const double wr = vr / denom;

  #if JACOBI
    // use separate array, v, for updated solution
    array v = u;
  #endif

  // outermost loop
  for (size_t iter = 0; iter < iterations; iter++) {

    std::chrono::time_point<std::chrono::steady_clock> timing_st = std::chrono::steady_clock::now();
    double dev = 0;
    double err = 0;
    #pragma omp parallel for
    for (size_t k = p; k < nz - p; k++) {
      double z = dx * (k - 0.5 * (nz - 1));
      for (size_t j = p; j < ny - p; j++) {
        double y = dy * (j - 0.5 * (ny - 1));
        for (size_t i = p; i < nx - p; i++) {
          double x = dz * (i - 0.5 * (nx - 1));
          double r = std::sqrt(x * x + y * y + z * z);
          double oldval = u(i, j, k);

          // Solve uxx + uyy + uzz = r where for 2nd order accuracy
          //
          //   uxx ~ (u(i-1, j, k) - 2 u(i, j, k) + u(i+1, j, k)) / dx^2
          //   uyy ~ (u(i, j-1, k) - 2 u(i, j, k) + u(i, j+1, k)) / dy^2
          //   uzz ~ (u(i, j, k-1) - 2 u(i, j, k) + u(i, j, k+1)) / dz^2
          //
          // Then
          //
          //   u(i, j, k) = (sx / dx^2 + sy / dy^2 + sz / dz^2 - r) /
          //                ( 2 / dx^2 +  2 / dy^2 +  2 / dz^2)
          // 2nd order accuracy
          double sx = fdnum[1] * (u(i - 1, j, k) + u(i + 1, j, k));
          double sy = fdnum[1] * (u(i, j - 1, k) + u(i, j + 1, k));
          double sz = fdnum[1] * (u(i, j, k - 1) + u(i, j, k + 1));
#if ORDER >= 4
          // 4th order accuracy
          sx += fdnum[2] * (u(i - 2, j, k) + u(i + 2, j, k));
          sy += fdnum[2] * (u(i, j - 2, k) + u(i, j + 2, k));
          sz += fdnum[2] * (u(i, j, k - 2) + u(i, j, k + 2));
#if ORDER >= 6
          // 6th order accuracy
          sx += fdnum[3] * (u(i - 3, j, k) + u(i + 3, j, k));
          sy += fdnum[3] * (u(i, j - 3, k) + u(i, j + 3, k));
          sz += fdnum[3] * (u(i, j, k - 3) + u(i, j, k + 3));
#if ORDER >= 8
          // 8th order accuracy
          sx += fdnum[4] * (u(i - 4, j, k) + u(i + 4, j, k));
          sy += fdnum[4] * (u(i, j - 4, k) + u(i, j + 4, k));
          sz += fdnum[4] * (u(i, j, k - 4) + u(i, j, k + 4));
#endif
#endif
#endif
          // compute new value u'(i, j, k)
          double newval = wx * sx + wy * sy + wz * sz + wr * r;

          // compute deviation between old and new value
          double d = newval - oldval;
          dev += d * d;

          // compute error in Laplacian relative to analytical solution
          double e = (vx * sx + vy * sy + vz * sz - denom * oldval) / fdden - r;
          err += e * e;

#if JACOBI
          // Jacobi: update intermediate state v
          v(i, j, k) = newval;
#else
          // Gauss-Seidel: update solution u immediately
          u(i, j, k) = newval;
#endif
        }
      }
    }
#if JACOBI
    // Jacobi: update solution u
    u = v;
#endif

    // end of iteration; print statistics
    dev = std::sqrt(dev / n);
    err = std::sqrt(err / n);
    std::chrono::time_point<std::chrono::steady_clock> timing_end = std::chrono::steady_clock::now();
    fprintf(stderr, "%zu %.5e %.5e %6.3f %zums\n", iter, dev, err, 8. * u.size_bytes(PAYLOAD) / u.size(), 
            std::chrono::duration_cast<std::chrono::milliseconds>(timing_end - timing_st).count());
  }

  // optionally compute and output Laplacian
  if (path) {
    array3<double> lap(nx - 2 * p, ny - 2 * p, nz - 2 * p);
    for (size_t k = p; k < nz - p; k++)
      for (size_t j = p; j < ny - p; j++)
        for (size_t i = p; i < nx - p; i++) {
          double val = u(i, j, k);
          // 2nd order accuracy
          double sx = fdnum[1] * (u(i - 1, j, k) + u(i + 1, j, k));
          double sy = fdnum[1] * (u(i, j - 1, k) + u(i, j + 1, k));
          double sz = fdnum[1] * (u(i, j, k - 1) + u(i, j, k + 1));
#if ORDER >= 4
          // 4th order accuracy
          sx += fdnum[2] * (u(i - 2, j, k) + u(i + 2, j, k));
          sy += fdnum[2] * (u(i, j - 2, k) + u(i, j + 2, k));
          sz += fdnum[2] * (u(i, j, k - 2) + u(i, j, k + 2));
#if ORDER >= 6
          // 6th order accuracy
          sx += fdnum[3] * (u(i - 3, j, k) + u(i + 3, j, k));
          sy += fdnum[3] * (u(i, j - 3, k) + u(i, j + 3, k));
          sz += fdnum[3] * (u(i, j, k - 3) + u(i, j, k + 3));
#if ORDER >= 8
          // 8th order accuracy
          sx += fdnum[4] * (u(i - 4, j, k) + u(i + 4, j, k));
          sy += fdnum[4] * (u(i, j - 4, k) + u(i, j + 4, k));
          sz += fdnum[4] * (u(i, j, k - 4) + u(i, j, k + 4));
#endif
#endif
#endif
          // compute Laplacian
          lap(i - p, j - p, k - p) = (vx * sx + vy * sy + vz * sz - denom * val) / fdden;
        }
    if (!lap.write(path))
      fprintf(stderr, "error writing file\n");
  }
}

int main(int argc, char* argv[])
{
  size_t n = 512; // number of grid cells per dimension
  size_t iterations = 3; //10000; // number of iterations
  char* path = 0;

  // Usage: poisson [iterations [grid-size [output-file]]]
  switch (argc) {
    case 4:
      path = argv[3];
      // FALLTHROUGH
    case 3:
      sscanf(argv[2], "%zu", &n);
      // FALLTHROUGH
    case 2:
      sscanf(argv[1], "%zu", &iterations);
      // FALLTHROUGH
    default:
      break;
  }
  

  // declare 3D solution array u
#if defined(ZFP_RATE)
  // zfp fixed-rate array
#if 1 //allocate a new array
#ifdef USE_UMAP  
  zfp::array3dumap u(n, n, n, ZFP_RATE);
#else
  zfp::array3d u(n, n, n, ZFP_RATE, 0, 8 * n * n * sizeof(double));
#endif
  //initialize u
  init(u);
#else
  /* Option 2: read from a compressed file */
  std::stringstream ss;
  ss<<"zfp3d_poisson128x128x128_rate16_step60000.bin";
  std::cout << ss.str() << std::endl;
  FILE * pFile = fopen ( ss.str().c_str() , "rb" );
  if (pFile==NULL) {fputs ("File error",stderr); exit (1);}

  fseek (pFile , 0 , SEEK_END);
  size_t lSize = (size_t) ftell (pFile);
  rewind (pFile);

  void* compressed_buffer = (void*) malloc (lSize);
  if (compressed_buffer == NULL) {fputs ("Memory error",stderr); exit (2);}

  size_t result = fread (compressed_buffer,1,lSize,pFile);
  if (result != lSize) {fputs ("Reading error",stderr); exit (3);}
  fclose (pFile);

  zfp::array3dumap u(n, n, n, ZFP_RATE, compressed_buffer);
#endif  
#elif defined(ZFP_TOLERANCE)
  // zfp variable-rate array
  zfp::var_array3d u(n, n, n, zfp_config_accuracy(ZFP_TOLERANCE), 0, 8 * n * n * sizeof(double));
  //initialize u
  init(u);
#else
  // scalar array
  array3<REAL> u(n, n, n);  
  //initialize u
  init(u);
#endif

  // run iterative solver
  iterate(u, iterations, path);

  return 0;
}
