//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2022 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include <unistd.h>
#include <stdio.h>
#include "StoreZFP.h"
#include <iostream>
#include <sstream>
#include <string.h>

#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"
#include <cassert>

// A global variable to ensure thread-safety
std::mutex mutRand;
static int num_xblocks_per_parititon;
static int num_yblocks_per_parititon=1;
static int num_zblocks_per_parititon=1;
static size_t num_partitions_x;
static size_t num_partitions_y;
static size_t num_partitions_z;
static size_t num_blocks_x;
static size_t num_blocks_y;
static size_t num_blocks_z;

namespace Umap {

  template <>
  StoreZFP<double>::StoreZFP(size_t _nx, size_t _ny, size_t _nz, double _rate, void* _compressed_buf)
  {
    if ( (read_env_var("UMAP_PAGESIZE", &psize)) == nullptr )
      psize = sysconf(_SC_PAGESIZE);

    assert( _rate>1.0 );

    rate = (int)_rate;
    nx = (_nx+3)/4 * 4;
    ny = (_ny+3)/4 * 4;
    nz = (_nz==1) ? 1 : ((_nz+3)/4 * 4);
    dim = (_nz==1) ? 2 : 3;
    size_t num_elements = nx*ny*nz;
    region_size = num_elements * sizeof(double);
    if( region_size< psize ){
      size_t scale = (psize+region_size-1)/region_size;
      ny *= scale;
      num_elements = nx*ny*nz;
      region_size = num_elements * sizeof(double);
    }
    assert( region_size % psize == 0 );

    size_t num_elements_per_page = psize/sizeof(double);
    elements_per_block = pow(4,dim);
    blocks_per_page = num_elements_per_page / elements_per_block;
    decodefunc = decodeFuncs[dim-1];
    encodefunc = encodeFuncs[dim-1];    

    size_t max_compressed_bytes = (num_elements*rate+7)/8;
    compressed_buffer = (char*)_compressed_buf;
    if( compressed_buffer==NULL ) compressed_buffer = (char*)malloc(max_compressed_bytes);

#if ZFP_THREAD_BARRIER
    /* open bit stream  */
    stream = stream_open(compressed_buffer, max_compressed_bytes);
  
    /* allocate meta data for a compressed stream */
    zfp = zfp_stream_open(stream);

    // Fixed-rate
    zfp_stream_set_rate(zfp, rate, zfp_type_double, dim, zfp_true);
    zfp_stream_rewind(zfp);
#endif

    UMAP_LOG(Info, "\n nx="<<nx<<", ny="<<ny<<", nz="<<nz<<", max_compressed_bytes="<<max_compressed_bytes);
  }

  template <>
  ssize_t StoreZFP<double>::read_from_store(char* buf, size_t nb, off_t off)
  {
    size_t page_id   = off / psize;
    size_t block_st  = page_id * blocks_per_page;
    size_t block_end = block_st + blocks_per_page;    

    #if ZFP_THREAD_BARRIER
    std::lock_guard<std::mutex> lk(mutRand); 
    #else
    /* open bit stream  */
    size_t bit_offset = block_st*elements_per_block*rate;
    assert( bit_offset%8 ==0 );
    bitstream* stream = stream_open(compressed_buffer + bit_offset/8, rate * nb/sizeof(double)/8);
  
    /* allocate meta data for a compressed stream */
    zfp_stream* zfp = zfp_stream_open(stream);

    // Fixed-rate
    zfp_stream_set_rate(zfp, rate, zfp_type_double, dim, zfp_true);
    zfp_stream_rewind(zfp);
    #endif

    double *ptr = (double*)buf;
    //stream_rseek(stream, offsets[block_st]);
    //stream_rseek(stream, block_st*elements_per_block*rate);
    for (size_t i = block_st; i < block_end; i++) {      
      int bits = decodefunc(zfp, ptr);
      //assert( bits/8==elements_per_block);
      ptr += elements_per_block;
      //printf("%dDblock #%u decoded offsets[%zu] bits=%4u\n",dim, i, i*elements_per_block*rate, bits);
    }

    #ifndef ZFP_THREAD_BARRIER
    /* clean up */
    zfp_stream_close(zfp);
    stream_close(stream);
    #endif

    return 0;
  }

  template <>
  ssize_t StoreZFP<double>::write_to_store(char* buf, size_t nb, off_t off)
  {
    size_t page_id   = off / psize;
    size_t block_st  = page_id * blocks_per_page;
    size_t block_end = block_st + blocks_per_page;    

    #if ZFP_THREAD_BARRIER
    std::lock_guard<std::mutex> lk(mutRand);
    #else
    /* open bit stream  */
    size_t bit_offset = block_st*elements_per_block*rate;
    assert( bit_offset%8 ==0 );
    bitstream* stream = stream_open(compressed_buffer + bit_offset/8, rate * nb/sizeof(double)/8);
  
    /* allocate meta data for a compressed stream */
    zfp_stream* zfp = zfp_stream_open(stream);

    // Fixed-rate
    zfp_stream_set_rate(zfp, rate, zfp_type_double, dim, zfp_true);
    zfp_stream_rewind(zfp);
    #endif

    double *ptr = (double*)buf;
    //stream_rseek(stream, offsets[block_st]);
    //stream_rseek(stream, block_st*elements_per_block*rate);
    for (size_t i = block_st; i < block_end; i++) {
      int bits = encodefunc(zfp, ptr);
      ptr += elements_per_block;
      //printf("%dDblock #%u encoded offsets[%zu] bits=%4u\n",dim, i, i*elements_per_block*rate, bits);
    }
    stream_flush(stream);

    #ifndef ZFP_THREAD_BARRIER
    /* clean up */
    zfp_stream_close(zfp);
    stream_close(stream);
    #endif

    return 0;
  }

  //TODO
  template <>
  StoreZFP<double>::~StoreZFP(){
    /* get pointer to compressed data for read or write access
    void* compressed_buf = zfp_store3->compressed_data();
    size_t buffer_size = zfp_store3->buffer_size();
    
    FILE * pFile;
    pFile = fopen ("zfp.bin", "wb");
    fwrite (compressed_buf , sizeof(double), buffer_size/sizeof(double), pFile);
    fclose (pFile);
    delete zfp_store3;
    */    
  }

  template <typename dtype>
  uint64_t* StoreZFP<dtype>::read_env_var(const char* env, uint64_t*  val)
  {
    // return a pointer to val on success, null on failure
    char* val_ptr = 0;
    if ( (val_ptr = getenv(env)) ) {
      uint64_t env_val;

      std::string s(val_ptr);
      std::stringstream ss(s);
      ss >> env_val;

      if (env_val != 0) {
        *val = env_val;
        return val;
      }
    }
    return nullptr;
  }

  // a dummy pref
  template <typename dtype>
  int StoreZFP<dtype>::predict_offsets(off_t* offsets, off_t off)
  { 
    offsets[0] = off;
    return 1;
  }


#ifdef ZFP_ARRAY_RAW
  template <>
  StoreZFP<double>::StoreZFP(size_t _nx, size_t _ny, double _rate, void* _compressed_buf){
    
    /* align the region size to umap page size */
    if ( (read_env_var("UMAP_PAGESIZE", &psize)) == nullptr )
      psize = sysconf(_SC_PAGESIZE);

    size_t dim_x = psize/sizeof(double);

    nx = (_nx + dim_x - 1)/dim_x * dim_x;
    ny = (_ny + 3)/4 * 4;
    region_size = nx*ny*sizeof(double);
    rate = _rate;

    /* use fixed rate for now */
    zfp_config config = zfp_config_rate(rate, true);
    zfp_store2 = new zfp::BlockStore2<double, zfp::codec::zfp2<double>, zfp::index::implicit>(nx, ny, config, _compressed_buf);

    UMAP_LOG(Info, "nx="<<nx<<", ny="<<ny<<", region_size="<<region_size);
  }

  template <>
  StoreZFP<double>::StoreZFP(size_t _nx, size_t _ny, size_t _nz, double _rate, void* _compressed_buf){
    
    // align the region size to umap page size
    if ( (read_env_var("UMAP_PAGESIZE", &psize)) == nullptr )
      psize = sysconf(_SC_PAGESIZE);

    size_t dim_x = psize/sizeof(double);

    nx = (_nx + dim_x - 1)/dim_x * dim_x;
    ny = (_ny + 3)/4 * 4;
    nz = (_nz + 3)/4 * 4;
    region_size = nx * ny * nz * sizeof(double);
    rate = _rate;

    // use fixed rate for now
    zfp_config config = zfp_config_rate(rate, true);
    zfp_store3 = new zfp::BlockStore3<double, zfp::codec::zfp3<double>, zfp::index::implicit>(nx, ny, nz, config, _compressed_buf);

    UMAP_LOG(Info, "nx="<<nx<<", ny="<<ny<<", nz="<<nz<<", region_size="<<region_size);
  }
  
  template <>
  StoreZFP<double>::~StoreZFP(){
    /* get pointer to compressed data for read or write access
    void* compressed_buf = zfp_store3->compressed_data();
    size_t buffer_size = zfp_store3->buffer_size();
    
    FILE * pFile;
    pFile = fopen ("zfp.bin", "wb");
    fwrite (compressed_buf , sizeof(double), buffer_size/sizeof(double), pFile);
    fclose (pFile);*/

    delete zfp_store3;
  }

  template<>
  int StoreZFP<double>::predict_offsets(off_t* offsets, off_t off)
  {
    
    size_t element_id = off/sizeof(double);
    size_t y = element_id / nx;
    size_t x = element_id - y*nx;

    size_t block_y = y/4;

    size_t dim_x = psize/sizeof(double);
    size_t page_x = x/dim_x*dim_x;

    int num_predict_pages = 0;
    assert((block_y*4 + 4)<=ny );
    for(int prefetch_step=0; prefetch_step<1; prefetch_step++)
    {
      if( (page_x+dim_x) <= nx ){
        for(int page = 0; page<4; page++){
          offsets[num_predict_pages++] = ((block_y*4 + page) * nx + page_x) * sizeof(double);
          //printf("predict_offsets: offset=%zu, block_y=%zu, page_x=%zu, offsets[%d]=%zu\n", off, block_y, page_x, page, offsets[page]);
        }
        page_x += dim_x*8;
      }else{
        break;
      }
    } 
    return num_predict_pages;
  }

    template <>
  ssize_t StoreZFP<double>::read_from_store(char* buf, size_t nb, off_t off)
  {
    std::lock_guard<std::mutex> lk(mutRand);  
    //printf("read_from_store_opt1: off=%zu, nb=%zu \n", off, nb);

    if( nb/psize == 4 ){
      
      size_t num_elements_per_page = psize/sizeof(double);
      size_t num_blocks_per_page = num_elements_per_page / 4;
      size_t num_blocks_x = nx / 4;

      size_t element_id = off/sizeof(double);
      size_t y = element_id / nx;
      size_t x = element_id % nx;

      size_t block_id = y / 4  * num_blocks_x + x / 4;   

      double* uncompressed_page_pointer = (double*) buf;

      for(size_t i = 0; i<num_blocks_per_page; i++){
        //printf("x=%zu, y=%zu, block_id=%zu\n", x, y, block_id);
        zfp_store2->decode(block_id, uncompressed_page_pointer, 1, num_elements_per_page);
        block_id ++;
        uncompressed_page_pointer += 4;
      }

      /* uncompressed_page_pointer = (double*) buf;
      for(size_t i = 0; i<num_elements_per_page*4; i++)
        printf("%.2f ", uncompressed_page_pointer[i]);
      */
    }else{
      UMAP_ERROR("nb/psize != 4");
    }

    return 0;
  }

  template <>
  ssize_t StoreZFP<double>::write_to_store(char* buf, size_t nb, off_t off)
  {  
    std::lock_guard<std::mutex> lk(mutRand);
    //printf("write_to_store_opt1: off=%zu, nb=%zu \n", off, nb);

    if( nb/psize == 4 ){
      
      size_t num_elements_per_page = psize/sizeof(double);
      size_t num_blocks_per_page = num_elements_per_page / 4;
      size_t num_blocks_x = nx / 4;

      size_t element_id = off/sizeof(double);
      size_t y = element_id / nx;
      size_t x = element_id % nx;

      size_t block_id = y / 4  * num_blocks_x + x / 4;   

      double* uncompressed_page_pointer = (double*) buf;

      for(size_t i = 0; i<num_blocks_per_page; i++){
        //printf("x=%zu, y=%zu, block_id=%zu, uncompressed_page_pointer=%p, (uncompressed_page_pointer+nx)=%p\n", 
        //        x, y, block_id, uncompressed_page_pointer, (uncompressed_page_pointer + nx));
        zfp_store2->encode(block_id, uncompressed_page_pointer, 1, nx);
        block_id ++;
        uncompressed_page_pointer += 4;
      }

      /* uncompressed_page_pointer = (double*) buf;
      for(size_t i = 0; i<num_elements_per_page*4; i++)
        printf("%.2f ", uncompressed_page_pointer[i]);
      */

    }else{
      UMAP_ERROR("nb/psize != 4");
    }

    return 0;
  }

#endif // ZFP_ARRAY_RAW


#ifdef ZFP_THREAD_PARTITION //use array of blocks and thread partition

  template <>
  StoreZFP<double>::StoreZFP(size_t _nx, size_t _ny, double _rate, void* _compressed_buf){
    
    /* align the region size to umap page size */
    if ( (read_env_var("UMAP_PAGESIZE", &psize)) == nullptr )
      psize = sysconf(_SC_PAGESIZE);

    size_t dim_x = psize/sizeof(double);
    num_xblocks_per_parititon = dim_x / 16;

    nx = (_nx + (4*num_xblocks_per_parititon)-1)/(4*num_xblocks_per_parititon) * (4*num_xblocks_per_parititon);
    ny = (_ny + (4*num_yblocks_per_parititon)-1)/(4*num_yblocks_per_parititon) * (4*num_yblocks_per_parititon);
    region_size = nx * ny * sizeof(double);
    rate = _rate;

    /* use fixed rate for now */
    zfp_config config = zfp_config_rate(rate, true);

    num_partitions_x = nx / 4 / num_xblocks_per_parititon;
    num_partitions_y = ny / 4 / num_yblocks_per_parititon;
    size_t num_partitions = num_partitions_x * num_partitions_y;

    num_blocks_x = nx / 4;
    num_blocks_y = ny / 4;

    UMAP_LOG(Info, "\n nx="<<nx<<", ny="<<ny<<
    "\n num_partitions_x="<<num_partitions_x<<", num_partitions_y="<<num_partitions_y<<
    "\n num_parititons="  <<num_partitions  <<", region_size="<<region_size);

    zfp_store2_array = (zfp::BlockStore2<double, zfp::codec::zfp2<double>, zfp::index::implicit>**) malloc(sizeof(zfp::BlockStore2<double, zfp::codec::zfp2<double>, zfp::index::implicit>*)*num_partitions);
    for(int i=0; i<num_partitions; i++){
      zfp_store2_array[i] = new zfp::BlockStore2<double, zfp::codec::zfp2<double>, zfp::index::implicit>( 4 * num_xblocks_per_parititon, 
                                                                                                          4 * num_yblocks_per_parititon,
                                                                                                          config, NULL);
    }

  }

  template <>
  StoreZFP<double>::StoreZFP(size_t _nx, size_t _ny, size_t _nz, double _rate, void* _compressed_buf){
    
    /* align the region size to umap page size */
    if ( (read_env_var("UMAP_PAGESIZE", &psize)) == nullptr )
      psize = sysconf(_SC_PAGESIZE);

    const size_t stride = psize/_rate;

    size_t dim_x = psize/sizeof(double);
    num_xblocks_per_parititon = dim_x / 64;

    nx = (_nx + (4*num_xblocks_per_parititon)-1)/(4*num_xblocks_per_parititon) * (4*num_xblocks_per_parititon);
    ny = (_ny + (4*num_yblocks_per_parititon)-1)/(4*num_yblocks_per_parititon) * (4*num_yblocks_per_parititon);
    nz = (_nz + (4*num_zblocks_per_parititon)-1)/(4*num_zblocks_per_parititon) * (4*num_zblocks_per_parititon);
    region_size = nx * ny * nz * sizeof(double);
    rate = _rate;

    /* use fixed rate for now */
    zfp_config config = zfp_config_rate(rate, true);    

    num_partitions_x = nx / 4 / num_xblocks_per_parititon;
    num_partitions_y = ny / 4 / num_yblocks_per_parititon;
    num_partitions_z = nz / 4 / num_zblocks_per_parititon;
    size_t num_partitions = num_partitions_x * num_partitions_y * num_partitions_z;

    num_blocks_x = nx / 4;
    num_blocks_y = ny / 4;
    num_blocks_z = nz / 4;
    UMAP_LOG(Info, "\n nx="<<nx<<", ny="<<ny<<", nz="<<nz<<
    "\n num_partitions_x="<<num_partitions_x<<", num_partitions_y="<<num_partitions_y<<", num_partitions_z="<<num_partitions_z<<
    "\n num_parititons="  <<num_partitions  <<", region_size="<<region_size);

    zfp_store3_array = (zfp::BlockStore3<double, zfp::codec::zfp3<double>, zfp::index::implicit>**) malloc(sizeof(zfp::BlockStore3<double, zfp::codec::zfp3<double>, zfp::index::implicit>*)*num_partitions);
    for(int i=0; i<num_partitions; i++){
      zfp_store3_array[i] = new zfp::BlockStore3<double, zfp::codec::zfp3<double>, zfp::index::implicit>( 4 * num_xblocks_per_parititon, 
                                                                                                          4 * num_yblocks_per_parititon,
                                                                                                          4 * num_zblocks_per_parititon, 
                                                                                                          config, compressed_buffer);
      if(compressed_buffer!=NULL)
        compressed_buffer += stride;
    }

  }

#ifdef ARRAY_2D
  template <>
  ssize_t StoreZFP<double>::read_from_store(char* buf, size_t nb, off_t off)
  {
    //std::lock_guard<std::mutex> lk(mutRand);

    size_t umap_page_id = off / psize;

    size_t num_elements_per_page = psize/sizeof(double);
    size_t num_blocks_per_page = num_elements_per_page / 16;

    size_t block_id = umap_page_id * num_blocks_per_page;

    size_t block_idy = block_id / num_blocks_x; block_id -= block_idy*num_blocks_x;
    size_t block_idx = block_id; 

    size_t partition_idy = block_idy / num_yblocks_per_parititon;
    size_t partition_idx = block_idx / num_xblocks_per_parititon;    
    size_t partition_id  = partition_idy*num_partitions_x + partition_idx;

    size_t partition_offy = block_idy % num_yblocks_per_parititon;
    size_t partition_offx = block_idx % num_xblocks_per_parititon;
    //partition-local block id
    size_t block_offset = partition_offy*num_xblocks_per_parititon + partition_offx;

    //printf("read: off=%zu, nb=%zu, block_id=%zu [%zu %zu %zu], partition_id=%zu [%zu %zu %zu], partition_off[%zu %zu %zu], block_offset=%zu\n", 
    //off, nb, block_id, block_idx,block_idy,block_idz, partition_id, partition_idx,partition_idy,partition_idz,partition_offx,partition_offy,partition_offz,block_offset);

    double* uncompressed_page_pointer = (double*) buf;
    // decompress all blocks that correspond to this page
    for (size_t i = 0; i < num_blocks_per_page; i++) {
      zfp_store2_array[partition_id]->decode(block_offset + i, uncompressed_page_pointer);
      uncompressed_page_pointer += 16;
    }
    return 0;
  }

  template <>
  ssize_t StoreZFP<double>::write_to_store(char* buf, size_t nb, off_t off)
  {
    //std::lock_guard<std::mutex> lk(mutRand);

    size_t umap_page_id = off / psize;

    size_t num_elements_per_page = psize/sizeof(double);
    size_t num_blocks_per_page = num_elements_per_page / 16;

    size_t block_id = umap_page_id * num_blocks_per_page;

    size_t block_idy = block_id / num_blocks_x; block_id -= block_idy*num_blocks_x;
    size_t block_idx = block_id; 

    size_t partition_idy = block_idy / num_yblocks_per_parititon;
    size_t partition_idx = block_idx / num_xblocks_per_parititon;    
    size_t partition_id  = partition_idy*num_partitions_x + partition_idx;

    size_t partition_offy = block_idy % num_yblocks_per_parititon;
    size_t partition_offx = block_idx % num_xblocks_per_parititon;
    //partition-local block id
    size_t block_offset = partition_offy*num_xblocks_per_parititon + partition_offx;

    //printf("write: off=%zu, nb=%zu, block_id=%zu [%zu %zu], partition_id=%zu [%zu %zu], partition_off[%zu %zu], block_offset=%zu\n", 
    //off, nb, block_id, block_idx,block_idy, partition_id, partition_idx,partition_idy,partition_offx,partition_offy,block_offset);

    double* uncompressed_page_pointer = (double*) buf;
    // compress all blocks in this page
    for (size_t i = 0; i < num_blocks_per_page; i++) {
      zfp_store2_array[partition_id]->encode(block_offset + i, uncompressed_page_pointer);
      uncompressed_page_pointer += 16;
    }
    return 0;
  }
#else //3D Array
  template <>
  ssize_t StoreZFP<double>::read_from_store(char* buf, size_t nb, off_t off)
  {
    //std::lock_guard<std::mutex> lk(mutRand);

    size_t umap_page_id = off / psize;

    size_t num_elements_per_page = psize/sizeof(double);
    size_t num_blocks_per_page = num_elements_per_page / 64;

    size_t block_id = umap_page_id * num_blocks_per_page;

    size_t block_idz = block_id / (num_blocks_x * num_blocks_y); block_id -= block_idz*(num_blocks_x * num_blocks_y);
    size_t block_idy = block_id / num_blocks_x; block_id -= block_idy*num_blocks_x;
    size_t block_idx = block_id; 

    size_t partition_idz = block_idz / num_zblocks_per_parititon;
    size_t partition_idy = block_idy / num_yblocks_per_parititon;
    size_t partition_idx = block_idx / num_xblocks_per_parititon;    
    size_t partition_id  = partition_idz*(num_partitions_x*num_partitions_y) + partition_idy*num_partitions_x + partition_idx;

    size_t partition_offz = block_idz % num_zblocks_per_parititon;
    size_t partition_offy = block_idy % num_yblocks_per_parititon;
    size_t partition_offx = block_idx % num_xblocks_per_parititon;
    //partition-local block id
    size_t block_offset = partition_offz * (num_xblocks_per_parititon*num_yblocks_per_parititon) + partition_offy*num_xblocks_per_parititon + partition_offx;

    //printf("read: off=%zu, nb=%zu, block_id=%zu [%zu %zu %zu], partition_id=%zu [%zu %zu %zu], partition_off[%zu %zu %zu], block_offset=%zu\n", 
    //off, nb, block_id, block_idx,block_idy,block_idz, partition_id, partition_idx,partition_idy,partition_idz,partition_offx,partition_offy,partition_offz,block_offset);

    double* uncompressed_page_pointer = (double*) buf;
    // decompress all blocks that correspond to this page
    for (size_t i = 0; i < num_blocks_per_page; i++) {
      //printf("read_from_storezfp3d_1d:3 block_id=%zu\n", block_id);
      zfp_store3_array[partition_id]->decode(block_offset + i, uncompressed_page_pointer);
      uncompressed_page_pointer += 64;
    }
    return 0;
  }

  template <>
  ssize_t StoreZFP<double>::write_to_store(char* buf, size_t nb, off_t off)
  {
    //std::lock_guard<std::mutex> lk(mutRand);

    size_t umap_page_id = off / psize;

    size_t num_elements_per_page = psize/sizeof(double);
    size_t num_blocks_per_page = num_elements_per_page / 64;

    size_t block_id = umap_page_id * num_blocks_per_page;

    size_t block_idz = block_id / (num_blocks_x * num_blocks_y); block_id -= block_idz*(num_blocks_x * num_blocks_y);
    size_t block_idy = block_id / num_blocks_x; block_id -= block_idy*num_blocks_x;
    size_t block_idx = block_id; 

    size_t partition_idz = block_idz / num_zblocks_per_parititon;
    size_t partition_idy = block_idy / num_yblocks_per_parititon;
    size_t partition_idx = block_idx / num_xblocks_per_parititon;    
    size_t partition_id  = partition_idz*(num_partitions_x*num_partitions_y) + partition_idy*num_partitions_x + partition_idx;

    size_t partition_offz = block_idz % num_zblocks_per_parititon;
    size_t partition_offy = block_idy % num_yblocks_per_parititon;
    size_t partition_offx = block_idx % num_xblocks_per_parititon;
    //partition-local block id
    size_t block_offset = partition_offz * (num_xblocks_per_parititon*num_yblocks_per_parititon) + partition_offy*num_xblocks_per_parititon + partition_offx;

    //printf("write: off=%zu, nb=%zu, block_id=%zu [%zu %zu %zu], partition_id=%zu [%zu %zu %zu], partition_off[%zu %zu %zu], block_offset=%zu\n", 
    //off, nb, block_id, block_idx,block_idy,block_idz, partition_id, partition_idx,partition_idy,partition_idz,partition_offx,partition_offy,partition_offz,block_offset);

    double* uncompressed_page_pointer = (double*) buf;
    // compress all blocks in this page
    for (size_t i = 0; i < num_blocks_per_page; i++) {
      zfp_store3_array[partition_id]->encode(block_offset + i, uncompressed_page_pointer);
      uncompressed_page_pointer += 64;
    }
    return 0;
  }
#endif //end of 3D
#endif //end of partition

#ifdef ZFP_THREAD_BARRIER  // use array of blocks and barrier
#ifdef ARRAY_2D
 template <>
  ssize_t StoreZFP<double>::read_from_store(char* buf, size_t nb, off_t off)
  {    
    std::lock_guard<std::mutex> lk(mutRand);
    //printf("read_from_store_opt2: off=%zu, nb=%zu \n", off, nb);

    size_t umap_page_id = off / psize;

    size_t num_elements_per_page = psize/sizeof(double);
    size_t num_blocks_per_page = num_elements_per_page / 16;

    size_t block_id = umap_page_id * num_blocks_per_page;

    //printf("read_from_store22: off=%zu, block_id=%zu\n", off, block_id);

    double* uncompressed_page_pointer = (double*) buf;
    size_t decoded_sz;
    // decompress all blocks that correspond to this page
    for (size_t i = 0; i < num_blocks_per_page; i++) {
      //printf("read_from_store22: block_id=%zu\n", block_id);
      decoded_sz = zfp_store2->decode(block_id, uncompressed_page_pointer, 1, 4);
      block_id++;
      uncompressed_page_pointer += 16;
    }
    
    /*
    double* buf2 = (double*) buf;
    size_t id = 0;
    block_id = umap_page_id * num_blocks_per_page;
    if(buf2[id] != 0.01)
    for (size_t i = 0; i < num_blocks_per_page; i++) {      
      printf("read_from_store22: block_id=%zu, decoded_sz=%zu, [%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f] \n"
            , block_id ++, decoded_sz
            , buf2[id++], buf2[id++], buf2[id++], buf2[id++]
            , buf2[id++], buf2[id++], buf2[id++], buf2[id++]
            , buf2[id++], buf2[id++], buf2[id++], buf2[id++]
            , buf2[id++], buf2[id++], buf2[id++], buf2[id++]);
    }*/

    return 0;
  }

  template <>
  ssize_t StoreZFP<double>::write_to_store(char* buf, size_t nb, off_t off)
  {
    //printf("write_to_store_opt2: off=%zu, nb=%zu \n", off, nb);
    std::lock_guard<std::mutex> lk(mutRand);

    size_t umap_page_id = off / psize;

    size_t num_elements_per_page = psize/sizeof(double);
    size_t num_blocks_per_page = num_elements_per_page / 16;

    size_t block_id = umap_page_id * num_blocks_per_page;

    //double buf_duplicated[num_elements_per_page];
    //memcpy( &(buf_duplicated[0]), buf, nb);
    //double* uncompressed_page_pointer = buf_duplicated;
    //double* buf_original = (double*)buf;
    //size_t  buf_original_id = 0;

    double* uncompressed_page_pointer = (double*) buf;
    // decompress all blocks that correspond to this page
    for (size_t i = 0; i < num_blocks_per_page; i++) {
      
      size_t encoded_sz = zfp_store2->encode(block_id, uncompressed_page_pointer, 1, 4);
      
      /*
      double buf_decoded[16];
      zfp_store2->decode(block_id, &(buf_decoded[0]), 1, 4);
      if( (buf_decoded[0]!=uncompressed_page_pointer[0]) || (buf_original[buf_original_id]!=0.01) ){
        size_t id = 0;
        printf("write_to_store22: block_id=%zu, uncompressed_page_pointer[%.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f] \n"
              , block_id
              , uncompressed_page_pointer[id++], uncompressed_page_pointer[id++], uncompressed_page_pointer[id++], uncompressed_page_pointer[id++]
              , uncompressed_page_pointer[id++], uncompressed_page_pointer[id++], uncompressed_page_pointer[id++], uncompressed_page_pointer[id++]
              , uncompressed_page_pointer[id++], uncompressed_page_pointer[id++], uncompressed_page_pointer[id++], uncompressed_page_pointer[id++]
              , uncompressed_page_pointer[id++], uncompressed_page_pointer[id++], uncompressed_page_pointer[id++], uncompressed_page_pointer[id++]);
        id = 0;
        printf("write_to_store22: block_id=%zu, buf_decoded[%.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f] \n\n"
              , block_id
              , buf_decoded[id++], buf_decoded[id++], buf_decoded[id++], buf_decoded[id++]
              , buf_decoded[id++], buf_decoded[id++], buf_decoded[id++], buf_decoded[id++]
              , buf_decoded[id++], buf_decoded[id++], buf_decoded[id++], buf_decoded[id++]
              , buf_decoded[id++], buf_decoded[id++], buf_decoded[id++], buf_decoded[id++]);
        id = 0;
        printf("write_to_store22: block_id=%zu, buf_original[%.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f] \n\n"
              , block_id
              , buf_original[buf_original_id++], buf_original[buf_original_id++], buf_original[buf_original_id++], buf_original[buf_original_id++]
              , buf_original[buf_original_id++], buf_original[buf_original_id++], buf_original[buf_original_id++], buf_original[buf_original_id++]
              , buf_original[buf_original_id++], buf_original[buf_original_id++], buf_original[buf_original_id++], buf_original[buf_original_id++]
              , buf_original[buf_original_id++], buf_original[buf_original_id++], buf_original[buf_original_id++], buf_original[buf_original_id++]);
        //UMAP_ERROR("compression failed!");
      }*/

      //printf("write_to_store22: block_id=%zu,  encoded_sz = %zu\n", block_id, encoded_sz);
      block_id++;
      uncompressed_page_pointer += 16;
    }

    return 0;
  }
#else // 3D array
  template <>
  ssize_t StoreZFP<double>::read_from_store(char* buf, size_t nb, off_t off)
  {
    std::lock_guard<std::mutex> lk(mutRand);

    size_t umap_page_id = off / psize;

    size_t num_elements_per_page = psize/sizeof(double);
    size_t num_blocks_per_page = num_elements_per_page / 64;

    size_t block_id = umap_page_id * num_blocks_per_page;
    //printf("read_from_storezfp3d_1d:2 off=%zu, nb=%zu, block_id=%zu\n", off, nb, block_id);

    double* uncompressed_page_pointer = (double*) buf;
    size_t decoded_sz;
    // decompress all blocks that correspond to this page
    for (size_t i = 0; i < num_blocks_per_page; i++) {
      //printf("read_from_storezfp3d_1d:3 block_id=%zu\n", block_id);
      decoded_sz = zfp_store3->decode(block_id, uncompressed_page_pointer);
      block_id++;
      uncompressed_page_pointer += 64;
    }
    return 0;
  }
  
  template <>
  ssize_t StoreZFP<double>::write_to_store(char* buf, size_t nb, off_t off)
  {
    std::lock_guard<std::mutex> lk(mutRand);

    size_t umap_page_id = off / psize;

    size_t num_elements_per_page = psize/sizeof(double);
    size_t num_blocks_per_page = num_elements_per_page / 64;

    size_t block_id = umap_page_id * num_blocks_per_page;
    //printf("write_to_storezfp3d_1d:2 off=%zu, nb=%zu, block_id=%zu\n", off, nb, block_id);

    double* uncompressed_page_pointer = (double*) buf;
    // compress all blocks in this page
    for (size_t i = 0; i < num_blocks_per_page; i++) {      
      size_t encoded_sz = zfp_store3->encode(block_id, uncompressed_page_pointer);
      block_id++;
      uncompressed_page_pointer += 64;
    }
    return 0;
  }
#endif //end of 3D
#endif //end of thread barrier impl

}
