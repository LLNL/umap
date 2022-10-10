#ifndef ZFP_ARRAY3UMAP_HPP
#define ZFP_ARRAY3UMAP_HPP

#include <cstddef>
#include <cstring>
#include <iterator>
#include <cassert>
#include "zfp/array.hpp"
#include "zfp/index.hpp"
#include "zfp/codec/zfpcodec.hpp"
#include "zfp/internal/array/cache3.hpp"
#include "zfp/internal/array/handle3.hpp"
#include "zfp/internal/array/iterator3.hpp"
#include "zfp/internal/array/pointer3.hpp"
#include "zfp/internal/array/reference3.hpp"
#include "zfp/internal/array/store3.hpp"
#include "zfp/internal/array/view3.hpp"

#include "umap/umap.h"
#include "umap/store/StoreZFP.h"

namespace zfp {

// compressed 3D array of scalars
template <
  typename Scalar,
  class Codec = zfp::codec::zfp3<Scalar>,
  class Index = zfp::index::implicit
>
class array3umap : public array {
private:
  Umap::StoreZFP<Scalar>* umap_store;
  void*  umap_uncompressed_base_ptr;
  size_t num_blocks_x;
  size_t num_blocks_y;
  size_t num_blocks_xy;
  size_t umap_psize;
  size_t num_blocks_per_page;
  double compression_rate;
  size_t compressed_bytes;
  void* compressed_buffer;   
public:
  // types utilized by nested classes
  typedef array3umap container_type;
  typedef Scalar value_type;
  typedef Codec codec_type;
  typedef Index index_type;
  typedef zfp::internal::BlockStore3<value_type, codec_type, index_type> store_type;
  typedef zfp::internal::BlockCache3<value_type, store_type> cache_type;
  typedef typename Codec::header header;

  // accessor classes
  typedef zfp::internal::dim3::const_reference<array3umap> const_reference;
  typedef zfp::internal::dim3::const_pointer<array3umap> const_pointer;
  typedef zfp::internal::dim3::const_iterator<array3umap> const_iterator;
  typedef zfp::internal::dim3::const_view<array3umap> const_view;
  typedef zfp::internal::dim3::private_const_view<array3umap> private_const_view;
  typedef zfp::internal::dim3::reference<array3umap> reference;
  typedef zfp::internal::dim3::pointer<array3umap> pointer;
  typedef zfp::internal::dim3::iterator<array3umap> iterator;
  typedef zfp::internal::dim3::view<array3umap> view;
  typedef zfp::internal::dim3::flat_view<array3umap> flat_view;
  typedef zfp::internal::dim3::nested_view1<array3umap> nested_view1;
  typedef zfp::internal::dim3::nested_view2<array3umap> nested_view2;
  typedef zfp::internal::dim3::nested_view2<array3umap> nested_view3;
  typedef zfp::internal::dim3::nested_view3<array3umap> nested_view;
  typedef zfp::internal::dim3::private_view<array3umap> private_view;

  // default constructor
  array3umap() :
    array(3, Codec::type)
    //cache(store) //umap version does not use software cache
  {}

  // constructor of nx * ny * nz array using rate bits per value
  array3umap(size_t nx, size_t ny, size_t nz, int rate=8) :
    array(3, Codec::type),
    compression_rate((double)rate),
    compressed_bytes((nx*ny*nz*rate+7)/8),
    compressed_buffer(NULL)
    //store(nx, ny, nz, zfp_config_rate(rate, true)),
    //cache(store, cache_size)
  {
    this->nx = nx;
    this->ny = ny;
    this->nz = nz;

    umap_store = new Umap::StoreZFP<Scalar>(nx, ny, nz, rate, compressed_buffer);
    umap_uncompressed_base_ptr = umap_ex(NULL, umap_store->get_region_size(), PROT_READ|PROT_WRITE, UMAP_PRIVATE, -1 , 0 , (Umap::Store*) umap_store);
    
    num_blocks_x = nx/4;
    num_blocks_y = ny/4;
    num_blocks_xy = num_blocks_x*num_blocks_y;
    umap_psize = umapcfg_get_umap_page_size();
    assert( umap_psize%(sizeof(Scalar)*64) == 0);
    num_blocks_per_page = umap_psize/sizeof(Scalar)/64;

    //TODO: setup page size and buffer size here
    printf("array3umap:: region_size = %zu, base_ptr %p\n", umap_store->get_region_size(), umap_uncompressed_base_ptr);
  }

  // constructor, from previously-serialized compressed array
  array3umap(const zfp::array::header& header, const void* buffer = 0, size_t buffer_size_bytes = 0) :
    array(3, Codec::type, header),
    compression_rate(header.rate()),
    compressed_bytes((nx*ny*nz*compression_rate+7)/8),
    compressed_buffer(buffer)
    //store(header.size_x(), header.size_y(), header.size_z(), zfp_config_rate(header.rate(), true)),
    //cache(store)
  {
    this->nx = header.size_x();
    this->ny = header.size_y();
    this->nz = header.size_z();
    assert( compression_rate > 1.0 );

    if (buffer) {
      if ( buffer_size_bytes != compressed_bytes )
        throw zfp::exception("buffer size differs from buffer_size_bytes");
    }
  }

/*
  // copy constructor--performs a deep copy
  array3(const array3& a) :
    array(),
    cache(store)
  {
    deep_copy(a);
  }

  // construction from view--perform deep copy of (sub)array
  template <class View>
  array3(const View& v) :
    array(3, Codec::type),
    store(v.size_x(), v.size_y(), v.size_z(), zfp_config_rate(v.rate(), true)),
    cache(store)
  {
    this->nx = v.size_x();
    this->ny = v.size_y();
    this->nz = v.size_z();
    // initialize array in its preferred order
    for (iterator it = begin(); it != end(); ++it)
      *it = v(it.i(), it.j(), it.k());
  }
*/

  // virtual destructor
  virtual ~array3umap() {
    if(umap_store) {
      uunmap(umap_uncompressed_base_ptr, umap_store->get_region_size());
      delete umap_store;
    }
  }

/*
  // assignment operator--performs a deep copy
  array3& operator=(const array3& a)
  {
    if (this != &a)
      deep_copy(a);
    return *this;
  }
*/

  // total number of elements in array
  size_t size() const { return nx * ny * nz; }

  // array dimensions
  size_t size_x() const { return nx; }
  size_t size_y() const { return ny; }
  size_t size_z() const { return nz; }

/*
  // resize the array (all previously stored data will be lost)
  void resize(size_t nx, size_t ny, size_t nz, bool clear = true)
  {
    cache.clear();
    this->nx = nx;
    this->ny = ny;
    this->nz = nz;
    store.resize(nx, ny, nz, clear);
  }
*/

  // rate in bits per value
  double rate() const { return compression_rate; }

/*
  // set rate in bits per value
  double set_rate(double rate)
  {
    cache.clear();
    return store.set_rate(rate, true);
  }
*/
  // byte size of array data structure components indicated by mask
  size_t size_bytes(uint mask = ZFP_DATA_ALL) const
  {
    size_t size = 0;
    //size += store.size_bytes(mask);
    //size += cache.size_bytes(mask);
    if (mask & ZFP_DATA_META)
      size += sizeof(*this);
    return size;
  }

  // number of bytes of compressed data
  size_t compressed_size() const { return compressed_bytes; }

  // pointer to compressed data for read or write access
  void* compressed_data() const
  {
    return compressed_buffer;
  }

/*
  // cache size in number of bytes
  size_t cache_size() const { return cache.size(); }

  // set minimum cache size in bytes (array dimensions must be known)
  void set_cache_size(size_t bytes)
  {
    cache.flush();
    cache.resize(bytes);
  }

  // empty cache without compressing modified cached blocks
  void clear_cache() const { cache.clear(); }

  // flush cache by compressing all modified cached blocks
  void flush_cache() const { cache.flush(); }

  // decompress array and store at p
  void get(value_type* p) const
  {
    const size_t bx = store.block_size_x();
    const size_t by = store.block_size_y();
    const size_t bz = store.block_size_z();
    const ptrdiff_t sx = 1;
    const ptrdiff_t sy = static_cast<ptrdiff_t>(nx);
    const ptrdiff_t sz = static_cast<ptrdiff_t>(nx * ny);
    size_t block_index = 0;
    for (size_t k = 0; k < bz; k++, p += 4 * sy * ptrdiff_t(ny - by))
      for (size_t j = 0; j < by; j++, p += 4 * sx * ptrdiff_t(nx - bx))
        for (size_t i = 0; i < bx; i++, p += 4)
          cache.get_block(block_index++, p, sx, sy, sz);
  }

  // initialize array by copying and compressing data stored at p
  void set(const value_type* p)
  {
    const size_t bx = store.block_size_x();
    const size_t by = store.block_size_y();
    const size_t bz = store.block_size_z();
    size_t block_index = 0;
    if (p) {
      // compress data stored at p
      const ptrdiff_t sx = 1;
      const ptrdiff_t sy = static_cast<ptrdiff_t>(nx);
      const ptrdiff_t sz = static_cast<ptrdiff_t>(nx * ny);
      for (size_t k = 0; k < bz; k++, p += 4 * sy * ptrdiff_t(ny - by))
        for (size_t j = 0; j < by; j++, p += 4 * sx * ptrdiff_t(nx - bx))
          for (size_t i = 0; i < bx; i++, p += 4)
            cache.put_block(block_index++, p, sx, sy, sz);
    }
    else {
      // zero-initialize array
      const value_type block[4 * 4 * 4] = {};
      while (block_index < bx * by * bz)
        cache.put_block(block_index++, block, 1, 4, 16);
    }
  }
*/
  // (i, j, k) accessors
  const_reference operator()(size_t i, size_t j, size_t k) const { return const_reference(const_cast<container_type*>(this), i, j, k); }
  reference operator()(size_t i, size_t j, size_t k) { return reference(this, i, j, k); }

  // flat index accessors
  const_reference operator[](size_t index) const
  {
    size_t i, j, k;
    ijk(i, j, k, index);
    return const_reference(const_cast<container_type*>(this), i, j, k);
  }
  reference operator[](size_t index)
  {
    size_t i, j, k;
    ijk(i, j, k, index);
    return reference(this, i, j, k);
  }

  // random access iterators
  const_iterator cbegin() const { return const_iterator(this, 0, 0, 0); }
  const_iterator cend() const { return const_iterator(this, 0, 0, nz); }
  const_iterator begin() const { return cbegin(); }
  const_iterator end() const { return cend(); }
  iterator begin() { return iterator(this, 0, 0, 0); }
  iterator end() { return iterator(this, 0, 0, nz); }

protected:
  friend class zfp::internal::dim3::const_handle<array3umap>;
  friend class zfp::internal::dim3::const_reference<array3umap>;
  friend class zfp::internal::dim3::const_pointer<array3umap>;
  friend class zfp::internal::dim3::const_iterator<array3umap>;
  friend class zfp::internal::dim3::const_view<array3umap>;
  friend class zfp::internal::dim3::private_const_view<array3umap>;
  friend class zfp::internal::dim3::reference<array3umap>;
  friend class zfp::internal::dim3::pointer<array3umap>;
  friend class zfp::internal::dim3::iterator<array3umap>;
  friend class zfp::internal::dim3::view<array3umap>;
  friend class zfp::internal::dim3::flat_view<array3umap>;
  friend class zfp::internal::dim3::nested_view1<array3umap>;
  friend class zfp::internal::dim3::nested_view2<array3umap>;
  friend class zfp::internal::dim3::nested_view3<array3umap>;
  friend class zfp::internal::dim3::private_view<array3umap>;
/*
  // perform a deep copy
  void deep_copy(const array3& a)
  {
    // copy base class members
    array::deep_copy(a);
    // copy persistent storage
    store.deep_copy(a.store);
    // copy cached data
    cache.deep_copy(a.cache);
  }
*/
  // global index bounds
  size_t min_x() const { return 0; }
  size_t max_x() const { return nx; }
  size_t min_y() const { return 0; }
  size_t max_y() const { return ny; }
  size_t min_z() const { return 0; }
  size_t max_z() const { return nz; }

  inline size_t get_umap_buffer_offset(size_t i, size_t j, size_t k) const{
    
    size_t block_idz = k >> 2;
    size_t block_idy = j >> 2;
    size_t block_idx = i >> 2;
    size_t block_offset_z = k % 4;
    size_t block_offset_y = j % 4;
    size_t block_offset_x = i % 4;
    size_t block_id = block_idz * num_blocks_xy + block_idy * num_blocks_x + block_idx;

    size_t umap_uncompressed_array_offset = (block_id<<6) + (block_offset_z<<4) + (block_offset_y<<2) + block_offset_x;
    //printf("umap_uncompressed_array_offset=%zu \n", umap_uncompressed_array_offset);

    return umap_uncompressed_array_offset;
  }

  // inspector
  value_type get(size_t i, size_t j, size_t k) const { 
    size_t off = get_umap_buffer_offset(i,j,k);
    return ((value_type*)umap_uncompressed_base_ptr)[off];
   }

  // mutators (called from proxy reference)
  void set(size_t i, size_t j, size_t k, value_type val) { 
    size_t off = get_umap_buffer_offset(i,j,k);
    ((value_type*)umap_uncompressed_base_ptr)[off] = val;
  }
  void add(size_t i, size_t j, size_t k, value_type val) { 
    size_t off = get_umap_buffer_offset(i,j,k);
    value_type v = ((value_type*)umap_uncompressed_base_ptr)[off];
    ((value_type*)umap_uncompressed_base_ptr)[off] = v + val; 
  }
  void sub(size_t i, size_t j, size_t k, value_type val) { 
    size_t off = get_umap_buffer_offset(i,j,k);
    value_type v = ((value_type*)umap_uncompressed_base_ptr)[off];
    ((value_type*)umap_uncompressed_base_ptr)[off] = v - val; 
  }
  void mul(size_t i, size_t j, size_t k, value_type val) { 
    size_t off = get_umap_buffer_offset(i,j,k); 
    value_type v = ((value_type*)umap_uncompressed_base_ptr)[off];
    ((value_type*)umap_uncompressed_base_ptr)[off] = v * val;
  }
  void div(size_t i, size_t j, size_t k, value_type val) { 
    size_t off = get_umap_buffer_offset(i,j,k);
    value_type v = ((value_type*)umap_uncompressed_base_ptr)[off];
    ((value_type*)umap_uncompressed_base_ptr)[off] = v / val; 
  }

  // convert flat index to (i, j, k)
  void ijk(size_t& i, size_t& j, size_t& k, size_t index) const
  {
    i = index % nx; index /= nx;
    j = index % ny; index /= ny;
    k = index;
  }

  //store_type store; // persistent storage of compressed blocks
  //cache_type cache; // cache of decompressed blocks
};

typedef array3umap<float> array3fumap;
typedef array3umap<double> array3dumap;

}

#endif
