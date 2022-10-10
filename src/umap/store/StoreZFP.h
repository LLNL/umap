//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_STORE_ZFP_H_
#define _UMAP_STORE_ZFP_H_
#include <cstdint>
#include "umap/store/Store.hpp"
#include "umap/umap.h"

#include "zfp.h"

namespace Umap {

  template <typename dtype>
  class StoreZFP : public Store {

    typedef size_t (*DecodeFunc) (zfp_stream* stream, double* block);
    typedef size_t (*EncodeFunc) (zfp_stream* stream, const double* block);
    DecodeFunc decodeFuncs[4] = 
    {
        zfp_decode_block_double_1,
        zfp_decode_block_double_2, 
        zfp_decode_block_double_3, 
        zfp_decode_block_double_4
    };
    EncodeFunc encodeFuncs[4] = 
    {
        zfp_encode_block_double_1,
        zfp_encode_block_double_2, 
        zfp_encode_block_double_3, 
        zfp_encode_block_double_4
    };  

  public:
    StoreZFP(size_t nx, size_t ny, double rate, void* compressed_buf=NULL);
    StoreZFP(size_t nx, size_t ny, size_t nz, double rate, void* compressed_buf=NULL);
    virtual ~StoreZFP();
    
    ssize_t get_region_size(){return region_size;}
    ssize_t get_region_ny(){return ny;}
    ssize_t get_region_nx(){return nx;}
    ssize_t read_from_store(char* buf, size_t nb, off_t off);
    ssize_t read_from_store_singleblock(char* buf, size_t nb, off_t off);
    ssize_t write_to_store(char* buf, size_t nb, off_t off);

    int predict_offsets(off_t* offsets, off_t off);

  protected:


  private:
    uint64_t* read_env_var( const char* env, uint64_t*  val );
    /*zfp::BlockStore2<dtype, zfp::codec::zfp2<dtype>, zfp::index::implicit> *zfp_store2;
    zfp::BlockStore2<dtype, zfp::codec::zfp2<dtype>, zfp::index::implicit>** zfp_store2_array;
    zfp::BlockStore3<dtype, zfp::codec::zfp3<dtype>, zfp::index::implicit> *zfp_store3;
    zfp::BlockStore3<dtype, zfp::codec::zfp3<dtype>, zfp::index::implicit>** zfp_store3_array;*/
    bitstream* stream;
    zfp_stream* zfp;
    size_t elements_per_block;
    size_t blocks_per_page;
    size_t nx;
    size_t ny;
    size_t nz;
    size_t region_size;
    size_t psize;
    char*  compressed_buffer;
    int rate;
    size_t dim;
    DecodeFunc decodefunc;
    EncodeFunc encodefunc;
  };

}

#endif
