#pragma once
#include <iostream>
#include <algorithm>
#include <string>
#include <chrono>
#include <atomic>

#include <stdlib.h>    // free
#include "zstd.h"



namespace Umap{
    inline std::pair<void*,size_t> compress(void* input_buffer, size_t input_buffer_size){
        size_t output_buffer_size_bound = ZSTD_compressBound(input_buffer_size);
        void* const output_buffer = malloc(output_buffer_size_bound);
        size_t output_size = ZSTD_compress(output_buffer, output_buffer_size_bound, input_buffer, input_buffer_size, 1);
        if (ZSTD_isError(output_size)){
            std::cerr << "Compression Error: - " << ZSTD_getErrorName(output_size) << std::endl;
            exit(-1);
        }
        return std::pair<void*,size_t>(output_buffer,output_size);
    }

    inline size_t decompress(void* input_buffer, void* output_buffer, size_t compressed_size){
        uint64_t rSize = ZSTD_getFrameContentSize(input_buffer, compressed_size);
        if (rSize == ZSTD_CONTENTSIZE_ERROR){
            std::cerr << "Decompression Error: File was not compressed by ZSTD" << std::endl;
            return -1;
        }

        if(rSize == ZSTD_CONTENTSIZE_UNKNOWN){
            std::cerr << "Decompression Error: File unable to get content size" << std::endl;
            return -1;
        }
        size_t decompressed_size =  ZSTD_decompress(output_buffer, rSize, input_buffer, compressed_size);
        return decompressed_size;
    }
}
