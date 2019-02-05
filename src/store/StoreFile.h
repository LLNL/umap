//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory
//
// Created by Marty McFadden, 'mcfadden8 at llnl dot gov'
// LLNL-CODE-733797
//
// All rights reserved.
//
// This file is part of UMAP.
//
// For details, see https://github.com/LLNL/umap
// Please also see the COPYRIGHT and LICENSE files for LGPL license.
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
