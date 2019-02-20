//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_STORE_FILE_H_
#define _UMAP_STORE_FILE_H_
#include <cstdint>
#include "umap/Store.h"
#include "umap/umap.h"

class StoreFile : public Store {
  public:
    StoreFile(void* _region_, size_t _rsize_, size_t _alignsize_, int _fd_);

    ssize_t read_from_store(char* buf, size_t nb, off_t off);
    ssize_t  write_to_store(char* buf, size_t nb, off_t off);
  private:
    void* region;
    void* alignment_buffer;
    size_t rsize;
    size_t alignsize;
    int fd;
};
#endif
