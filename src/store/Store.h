/*
 * This file is part of UMAP.  For copyright information see the COPYRIGHT
 * file in the top level directory, or at
 * https://github.com/LLNL/umap/blob/master/COPYRIGHT.
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License (as published by
 * the Free Software Foundation) version 2.1 dated February 1999.  This program
 * is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the terms and conditions of the GNU Lesser General
 * Public License for more details.  You should have received a copy of the
 * GNU Lesser General Public License along with this program; if not, write to
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA
 */
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
