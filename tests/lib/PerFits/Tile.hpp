/*
 * This file is part of UMAP.
 *
 * For copyright information see the COPYRIGHT file in the top level
 * directory or at https://github.com/LLNL/umap/blob/master/COPYRIGHT.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License (as published by
 * the Free Software Foundation) version 2.1 dated February 1999.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA
 */
#ifndef _FITS_TILE_H
#define _FITS_TILE_H
#include <ostream>
#include <string>
#include <vector>

namespace Fits {

struct Tile_Dim {
  std::size_t xDim;
  std::size_t yDim;
  std::size_t elem_size;
};

struct Tile_File {
  int fd;
  std::string fname;
  std::size_t tile_start;
  std::size_t tile_size;
};

class Tile {
friend std::ostream &operator<<(std::ostream &os, Fits::Tile const &ft);
public:
  Tile(const std::string& _fn, bool use_direct_io);
  ssize_t pread(std::size_t alignment, void* cpy_buf, void* buf, std::size_t nbytes, off_t offset);
  Tile_Dim get_Dim() { return dim; }
private:
  Tile_File file;
  Tile_Dim  dim;
};

std::ostream &operator<<(std::ostream &os, Fits::Tile const &ft);
}

#endif
