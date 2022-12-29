#ifndef SOLVER_HPP
#define SOLVER_HPP

#ifdef _OPENMP
#if POISSON_WITH_ZFP && !JACOBI
  #error "must use -DJACOBI with -DPOISSON_WITH_ZFP and OpenMP"
#endif
  #include <omp.h>
#endif

#include <chrono>

#define GHOST_LAYERS 4 // # ghost layers sufficient for differential operator

// use Jacobi (when defined) or Gauss-Seidel update (set via Makefile)
// #define JACOBI 1

// initialize with analytical solution (set via Makefile)
// #define INIT_CHEAT 1

// storage mask for querying array payload
#ifdef ZFP_DATA_PAYLOAD
  #define PAYLOAD ZFP_DATA_PAYLOAD
#else
  #define PAYLOAD 0
#endif

// generic Poisson equation solver for conventional scalar arrays
template <
  class array, // array representation of solution
  int order    // finite-difference order of accuracy {2, 4, 6, 8}
>
class PoissonSolverBase {
public:
  PoissonSolverBase(array& u) :
    ng(GHOST_LAYERS),
    nx(u.size_x()),
    ny(u.size_y()),
    nz(u.size_z()),
    mx(nx - 2 * ng),
    my(ny - 2 * ng),
    mz(nz - 2 * ng),
    dx(2. / mx),
    dy(2. / my), 
    dz(2. / mz), 
    u(u)
  {}

  // set initial conditions
  virtual void init()
  {
    for (size_t k = 0; k < nz; k++) {
      double z = coord(k, dz);
      for (size_t j = 0; j < ny; j++) {
        double y = coord(j, dy);
        for (size_t i = 0; i < nx; i++) {
          double x = coord(i, dx);
          u(i, j, k) = initial(i, j, k, x, y, z);
        }
      }
    }
  }

  // solve Poisson equation using Jacobi or Gauss-Seidel iteration
  void solve(size_t iterations)
  {
#if JACOBI
    // Jacobi: use separate array, v, for updated solution
    array v = u;
#else
    // Gauss-Seidel: update solution u immediately (v is an alias for u)
    array& v = u;
#endif

    // outermost loop
    for (size_t iter = 0; iter < iterations; iter++) {
      
      std::chrono::time_point<std::chrono::steady_clock> timing_st = std::chrono::steady_clock::now();
      // advance solution one time step from u to v
      double diff = advance(v);
      std::chrono::time_point<std::chrono::steady_clock> timing_end = std::chrono::steady_clock::now();
      printf("%zu ms\n",std::chrono::duration_cast<std::chrono::milliseconds>(timing_end - timing_st).count());
      
#if JACOBI
      // Jacobi: update solution u
      u = v;
#endif

      // compute error in solution and Laplacian
      std::pair<double, double> err = error();

      // end of iteration; print statistics
      double rate = 8. * u.size_bytes(PAYLOAD) / u.size();
      fprintf(stderr, "%zu %.5e %.5e %.5e %6.3f\n", iter, diff, err.first, err.second, rate);
    }
  }

  // output solution and/or its Laplacian
  void output(const char* upath, const char* ddupath) const
  {
    // optionally compute and output Laplacian
    if (upath) {
      array3<double> a(mx, my, mz);
      for (size_t k = ng; k < nz - ng; k++)
        for (size_t j = ng; j < ny - ng; j++)
          for (size_t i = ng; i < nx - ng; i++)
            a(i - ng, j - ng, k - ng) = u(i, j, k);
      if (!a.write(upath))
        fprintf(stderr, "error writing file '%s'\n", upath);
    }

    // optionally compute and output Laplacian
    if (ddupath) {
      LaplaceOperator<array, order> ddu(u, ng);
      array3<double> a(mx, my, mz);
      for (size_t k = ng; k < nz - ng; k++)
        for (size_t j = ng; j < ny - ng; j++)
          for (size_t i = ng; i < nx - ng; i++)
            a(i - ng, j - ng, k - ng) = ddu(i, j, k);
      if (!a.write(ddupath))
        fprintf(stderr, "error writing file '%s'\n", ddupath);
    }
  }

protected:
  // analytical solution
  static double f(double x, double y, double z)
  {
    double r = std::sqrt(x * x + y * y + z * z);
    return r * r * r / 12;
  }

  // analytical Laplacian
  static double ddf(double x, double y, double z)
  {
    return std::sqrt(x * x + y * y + z * z);
  }

  // coordinate corresponding to grid point i with grid spacing h
  double coord(int i, double h) const { return (i - int(ng) + 0.5) * h - 1; }

  // initial conditions at grid point (i, j, k)
  double initial(size_t i, size_t j, size_t k, double x, double y, double z) const
  {
#if INIT_CHEAT
    // cheat by initializing u to analytical solution
    return f(x, y, z);
#else
    // initialize interior to zero, boundary to analytical solution
    return (ng <= i && i < nx - ng &&
            ng <= j && j < ny - ng &&
            ng <= k && k < nz - ng) ? 0.0 : f(x, y, z);
#endif
  }

  // advance solution u one time step to v (may be an alias for u)
  virtual double advance(array& v) const
  {
    double diff = 0;
    LaplaceOperator<array, order> ddu(u, ng);

#ifdef _OPENMP
    #pragma omp parallel for reduction(+:diff)
#endif
    for (size_t k = ng; k < nz - ng; k++) {
      double z = coord(k, dz);
      for (size_t j = ng; j < ny - ng; j++) {
        double y = coord(j, dy);
        for (size_t i = ng; i < nx - ng; i++) {
          double x = coord(i, dx);
          double r = ddf(x, y, z);
          double uold = u(i, j, k);

          // compute new value u'(i, j, k)
          double unew = ddu.solve(i, j, k, r);
          v(i, j, k) = unew;

          // compute difference between old and new value
          double d = unew - uold;
          diff += d * d;
        }
      }
    }

    return std::sqrt(diff / (mx * my * mz));
  }

  // error in solution and Laplacian
  virtual std::pair<double, double> error() const
  {
    double esol = 0; // error in solution
    double elap = 0; // error in Laplacian
    LaplaceOperator<array, order> ddu(u, ng);

#ifdef _OPENMP
    #pragma omp parallel for reduction(+:esol,elap)
#endif
    for (size_t k = ng; k < nz - ng; k++) {
      double z = coord(k, dz);
      for (size_t j = ng; j < ny - ng; j++) {
        double y = coord(j, dy);
        for (size_t i = ng; i < nx - ng; i++) {
          double x = coord(i, dx);

          // compute error in u relative to analytical solution
          double unew = u(i, j, k);
          double utrue = f(x, y, z);
          esol += (unew - utrue) * (unew - utrue);

          // compute error in Laplacian relative to analytical solution
          double Lnew = ddu(i, j, k);
          double Ltrue = ddf(x, y, z);
          elap += (Lnew - Ltrue) * (Lnew - Ltrue);
        }
      }
    }

    esol = std::sqrt(esol / (mx * my * mz));
    elap = std::sqrt(elap / (mx * my * mz));

    return std::pair<double, double>(esol, elap);
  }

  const size_t ng;         // number of ghost layers in each direction
  const size_t nx, ny, nz; // grid dimensions including ghost layers
  const size_t mx, my, mz; // grid dimensions without ghost layers
  const double dx, dy, dz; // grid spacing
  array& u;                // solution array
};

// generic solver
template <class array, int order>
class PoissonSolver : public PoissonSolverBase<array, order> {
public:
  PoissonSolver(array& u) : PoissonSolverBase<array, order>(u) {}
  virtual ~PoissonSolver() {}
};

#if POISSON_WITH_ZFP
// solver specialized for zfp arrays
template <int order>
class PoissonSolver<zfp::array3d, order> : public PoissonSolverBase<zfp::array3d, order> {
public:
  PoissonSolver(zfp::array3d& u) : PoissonSolverBase<zfp::array3d, order>(u) {}
  virtual ~PoissonSolver() {}

  // set initial conditions
  virtual void init()
  {
    // initialize zfp array u(x, y, z) using iterator to promote accuracy
    for (zfp::array3d::iterator p = u.begin(); p != u.end(); p++) {
      size_t i = p.i(); double x = coord(i, dx);
      size_t j = p.j(); double y = coord(j, dy);
      size_t k = p.k(); double z = coord(k, dz);
      *p = initial(i, j, k, x, y, z);
    }
  }

protected:
// use thread-safe zfp array views with OpenMP
#ifdef _OPENMP
  using PoissonSolverBase<zfp::array3d, order>::f;
  using PoissonSolverBase<zfp::array3d, order>::ddf;
  using PoissonSolverBase<zfp::array3d, order>::coord;
  using PoissonSolverBase<zfp::array3d, order>::initial;
  using PoissonSolverBase<zfp::array3d, order>::ng;
  using PoissonSolverBase<zfp::array3d, order>::nx;
  using PoissonSolverBase<zfp::array3d, order>::ny;
  using PoissonSolverBase<zfp::array3d, order>::nz;
  using PoissonSolverBase<zfp::array3d, order>::mx;
  using PoissonSolverBase<zfp::array3d, order>::my;
  using PoissonSolverBase<zfp::array3d, order>::mz;
  using PoissonSolverBase<zfp::array3d, order>::dx;
  using PoissonSolverBase<zfp::array3d, order>::dy;
  using PoissonSolverBase<zfp::array3d, order>::dz;
  using PoissonSolverBase<zfp::array3d, order>::u;

  // advance solution u one time step to v (parallel zfp array implementation)
  virtual double advance(zfp::array3d& v) const
  {
    double diff = 0;
    // flush shared cache to ensure cache consistency across threads
    u.flush_cache();
    // update subdomains of v in parallel
    #pragma omp parallel reduction(+:diff)
    {
      // create read-only private view of entire array u
      zfp::array3d::private_const_view myu(&u);
      LaplaceOperator<zfp::array3d::private_const_view, order> ddu(myu, ng);
      // create read-write private view into rectangular subset of v
      zfp::array3d::private_view myv(&v);
      myv.partition(omp_get_thread_num(), omp_get_num_threads());
#if 0
      // process subdomain assigned to this thread using indices
      for (size_t k = 0; k < myv.size_z(); k++) {
        size_t kk = myv.global_z(k);
        if (ng <= kk && kk < nz - ng) {
          double z = coord(kk, dz);
          for (size_t j = 0; j < myv.size_y(); j++) {
            size_t jj = myv.global_y(j);
            if (ng <= jj && jj < ny - ng) {
              double y = coord(jj, dy);
              for (size_t i = 0; i < myv.size_x(); i++) {
                size_t ii = myv.global_x(i);
                if (ng <= ii && ii < nx - ng) {
                  double x = coord(ii, dx);
                  double r = ddf(x, y, z);
                  double uold = myu(ii, jj, kk);

                  // compute new value u'(i, j, k)
                  double unew = ddu.solve(ii, jj, kk, r);
                  myv(i, j, k) = unew;

                  // compute difference between old and new value
                  double d = unew - uold;
                  diff += d * d;
                }
              }
            }
          }
        }
      }
#else
      // process subdomain assigned to this thread using iterator
      for (zfp::array3d::private_view::iterator p = myv.begin(); p != myv.end(); p++) {
        size_t i = myv.global_x(p.i());
        size_t j = myv.global_y(p.j());
        size_t k = myv.global_z(p.k());
        if (ng <= i && i < nx - ng &&
            ng <= j && j < ny - ng &&
            ng <= k && k < nz - ng) {
          double x = coord(i, dx);
          double y = coord(j, dy);
          double z = coord(k, dz);
          double r = ddf(x, y, z);
          double uold = myu(i, j, k);

          // compute new value u'(i, j, k)
          double unew = ddu.solve(i, j, k, r);
          *p = unew;

          // compute difference between old and new value
          double d = unew - uold;
          diff += d * d;
        }
      }
#endif

      // compress all private cached blocks to shared storage
      myv.flush_cache();
    }

    return std::sqrt(diff / (mx * my * mz));
  }

  // error in solution and Laplacian
  virtual std::pair<double, double> error() const
  {
    double esol = 0; // error in solution
    double elap = 0; // error in Laplacian

    #pragma omp parallel reduction(+:esol,elap)
    {
      // create read-only private view of entire array u
      zfp::array3d::private_const_view myu(&u);
      LaplaceOperator<zfp::array3d::private_const_view, order> ddu(myu, ng);
      // determine outer loop iterations assigned to this thread
      size_t kmin = ng + mz * (omp_get_thread_num() + 0) / omp_get_num_threads();
      size_t kmax = ng + mz * (omp_get_thread_num() + 1) / omp_get_num_threads();
      // process subdomain assigned to this thread
      for (size_t k = kmin; k < kmax; k++) {
        double z = coord(k, dz);
        for (size_t j = ng; j < ny - ng; j++) {
          double y = coord(j, dy);
          for (size_t i = ng; i < nx - ng; i++) {
            double x = coord(i, dx);

            // compute error in u relative to analytical solution
            double unew = myu(i, j, k);
            double utrue = f(x, y, z);
            esol += (unew - utrue) * (unew - utrue);

            // compute error in Laplacian relative to analytical solution
            double Lnew = ddu(i, j, k);
            double Ltrue = ddf(x, y, z);
            elap += (Lnew - Ltrue) * (Lnew - Ltrue);
          }
        }
      }
    }

    esol = std::sqrt(esol / (mx * my * mz));
    elap = std::sqrt(elap / (mx * my * mz));

    return std::pair<double, double>(esol, elap);
  }
#endif
};
#endif

#endif
