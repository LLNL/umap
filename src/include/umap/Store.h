//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
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
