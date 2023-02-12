#ifndef LAPLACE_HPP
#define LAPLACE_HPP

// finite difference Laplace operator of given order of accuracy
template <class array, int order>
class LaplaceOperator {
public:
  // constructor for mx * my * mz grid with 2 ng additional ghost layers
  LaplaceOperator(const array& u, size_t ng) :
    // grid spacing
    dx(2. / (u.size_x() - 2 * ng)),
    dy(2. / (u.size_y() - 2 * ng)),
    dz(2. / (u.size_z() - 2 * ng)),
    // operator order and number of ghost layers
    u(u),
    p(order / 2 - 1),
    // unnormalized inner-product weights
    vx(1. / (dx * dx)),
    vy(1. / (dy * dy)),
    vz(1. / (dz * dz)),
    vr(-den[p]),
    // normalized inner-product weights
    wx(vx / (-num[p][0] * (vx + vy + vz))),
    wy(vy / (-num[p][0] * (vx + vy + vz))),
    wz(vz / (-num[p][0] * (vx + vy + vz))),
    wr(vr / (-num[p][0] * (vx + vy + vz)))
  {}

  // finite-difference approximation to Laplacian div(grad(u(i, j, k)))
  double operator()(size_t i, size_t j, size_t k) const
  {
    double val = u(i, j, k);
    double sx, sy, sz;
    stencil(i, j, k, sx, sy, sz);
    return (vx * (num[p][0] * val + sx) +
            vy * (num[p][0] * val + sy) +
            vz * (num[p][0] * val + sz)) / den[p];
  }

  // solve for u(i, j, k) such that uxx + uyy + uzz = r
  double solve(size_t i, size_t j, size_t k, double r) const
  {
    // Solve uxx + uyy + uzz = r where (for 2nd order accuracy)
    //
    //   uxx ~ (u(i-1, j, k) - 2 u(i, j, k) + u(i+1, j, k)) / dx^2
    //   uyy ~ (u(i, j-1, k) - 2 u(i, j, k) + u(i, j+1, k)) / dy^2
    //   uzz ~ (u(i, j, k-1) - 2 u(i, j, k) + u(i, j, k+1)) / dz^2
    //
    // Let
    //
    //   sx = u(i-1, j, k) + u(i+1, j, k)
    //   sy = u(i, j-1, k) + u(i, j+1, k)
    //   sz = u(i, j, k-1) + u(i, j, k+1)
    //
    // Then
    //
    //   u(i, j, k) = (sx / dx^2 + sy / dy^2 + sz / dz^2 - r) /
    //                ( 2 / dx^2 +  2 / dy^2 +  2 / dz^2)

    double sx, sy, sz;
    stencil(i, j, k, sx, sy, sz);
    return wx * sx + wy * sy + wz * sz + wr * r;
  }

  const double dx, dy, dz;

protected:
  // compute finite difference components (sx, sy, sz)
  void stencil(size_t i, size_t j, size_t k, double& sx, double& sy, double& sz) const
  {
    sx = sy = sz = 0;
    if (order >= 2) {
      // 2nd order accuracy
      sx += num[p][1] * (u(i - 1, j, k) + u(i + 1, j, k));
      sy += num[p][1] * (u(i, j - 1, k) + u(i, j + 1, k));
      sz += num[p][1] * (u(i, j, k - 1) + u(i, j, k + 1));
    }
    if (order >= 4) {
      // 4th order accuracy
      sx += num[p][2] * (u(i - 2, j, k) + u(i + 2, j, k));
      sy += num[p][2] * (u(i, j - 2, k) + u(i, j + 2, k));
      sz += num[p][2] * (u(i, j, k - 2) + u(i, j, k + 2));
    }
    if (order >= 6) {
      // 6th order accuracy
      sx += num[p][3] * (u(i - 3, j, k) + u(i + 3, j, k));
      sy += num[p][3] * (u(i, j - 3, k) + u(i, j + 3, k));
      sz += num[p][3] * (u(i, j, k - 3) + u(i, j, k + 3));
    }
    if (order >= 8) {
      // 8th order accuracy
      sx += num[p][4] * (u(i - 4, j, k) + u(i + 4, j, k));
      sy += num[p][4] * (u(i, j - 4, k) + u(i, j + 4, k));
      sz += num[p][4] * (u(i, j, k - 4) + u(i, j, k + 4));
    }
  }

  static const int num[4][5];
  static const int den[4];

  const array& u;
  const int p;
  const double vx, vy, vz, vr;
  const double wx, wy, wz, wr;
};

// 1D finite-difference stencils for various orders of accuracy
template <class array, int order>
const int LaplaceOperator<array, order>::num[4][5] = {
  {     -2,    1                 }, // 2nd order
  {    -30,   16,    -1          }, // 4th order
  {   -490,  270,   -27,   2     }, // 6th order
  { -14350, 8064, -1008, 128, -9 }, // 8th order
};

// 1D finite-difference stencil common denominator
template <class array, int order>
const int LaplaceOperator<array, order>::den[4] = {
     1, // 2nd order
    12, // 4th order
   180, // 6th order
  5040, // 8th order
};

#endif
