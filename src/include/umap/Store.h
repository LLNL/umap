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
#ifndef _UMAP_STORE_H_
#define _UMAP_STORE_H_
#include <unistd.h>
#include <cstdint>

class Store {
  public:
    static Store* make_store(void* _region_, std::size_t _rsize_, std::size_t _alignsize_, int _fd_);

    virtual ssize_t read_from_store(char* buf, std::size_t nb, off_t off) = 0;
    virtual ssize_t  write_to_store(char* buf, std::size_t nb, off_t off) = 0;
};
#endif
