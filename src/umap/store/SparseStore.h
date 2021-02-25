//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2021 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

#include <cstdint>
#include <atomic>
#include "umap/store/Store.hpp"
#include "umap/umap.h"

namespace Umap {
  class SparseStore : public Store {
  public:
    SparseStore(size_t _rsize_, size_t _aligned_size_, std::string _root_path_, size_t _file_Size_);
    SparseStore(std::string _root_path, bool _read_only_);
    ~SparseStore();
    ssize_t read_from_store(char* buf, size_t nb, off_t off);
    ssize_t write_to_store(char* buf, size_t nb, off_t off);
    size_t get_current_capacity();
    static size_t get_capacity(std::string base_path);
    int close_files();
  private:
    int fd;
    size_t file_size;
    size_t current_capacity;
    uint64_t num_files;
    size_t rsize;
    size_t aligned_size;
    bool read_only;
    std::string root_path;
    // reads and writes I/O counters
    std::atomic<int64_t> numreads;
    std::atomic<int64_t> numwrites;
    struct file_descriptor{
      int id;
      off_t beginning;
      off_t end;
    };
    file_descriptor* file_descriptors; 
    std::mutex creation_mutex;
    int get_fd(off_t offset, off_t &file_offset);
    // ssize_t get_file_size(const std::string file_path);
  };
}
