#ifndef ARRAY_HPP
#define ARRAY_HPP

// linear 3D array templated on scalar type
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
  ~array3() { delete[] data; }

  // copy constructor (deep copy)
  array3(const array3& that) : data(0) { copy(that); }

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

#endif
