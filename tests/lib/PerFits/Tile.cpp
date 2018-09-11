/*
 * This file is part of UMAP.
 
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
#include <iostream>
#include <ostream>
#include <string>
#include <cassert>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "Tile.hpp"
#include "fitsio.h"
#include "spindle_debug.h"

namespace Fits {
Tile::Tile(const std::string& _fn, bool use_direct_io)
{
  fitsfile* fptr = NULL;
  int status = 0;
  LONGLONG headstart;
  LONGLONG datastart;
  LONGLONG dataend;
  int bitpix;
  long naxis[2];
  int naxes;
  int open_flags = (O_RDONLY | O_LARGEFILE);

  if (use_direct_io)
    open_flags |= O_DIRECT;

  file.fname = _fn;
  file.tile_start = (size_t)0;
  file.tile_size = (size_t)0;
  dim.xDim = (size_t)0;
  dim.yDim = (size_t)0;
  dim.elem_size = 0;
  file.fd = -1;

  if ( fits_open_data(&fptr, file.fname.c_str(), READONLY, &status) ) {
    fits_report_error(stderr, status);
    exit(-1);
  }

  if ( fits_get_hduaddrll(fptr, &headstart, &datastart, &dataend, &status) ) {
    fits_report_error(stderr, status);
    exit(-1);
  }

  if ( fits_get_img_type(fptr, &bitpix, &status) ) {
    fits_report_error(stderr, status);
    exit(-1);
  }

  if ( fits_get_img_param(fptr, 2, &bitpix, &naxes, &naxis[0], &status) ) {
    fits_report_error(stderr, status);
    exit(-1);
  }

  if ( fits_close_file(fptr, &status) ) {
    fits_report_error(stderr, status);
    exit(-1);
  }

  if ( ( file.fd = open(file.fname.c_str(), open_flags) ) == -1 ) {
    perror(file.fname.c_str());
    exit(-1);
  }

  dim.xDim = (size_t)naxis[0];
  dim.yDim = (size_t)naxis[1];
  dim.elem_size = bitpix < 0 ? (size_t)( ( bitpix * -1 ) / 8 ) : (size_t)( bitpix / 8 );
  file.tile_start = (size_t)datastart;
  file.tile_size = (size_t)(dim.xDim * dim.yDim * dim.elem_size);

  assert( (dataend - datastart) >= (dim.xDim * dim.yDim * dim.elem_size) );
}

ssize_t Tile::pread(std::size_t alignment, void* cpy_buf, void* buf, std::size_t nbytes, off_t offset)
{
  std::size_t rval;

  offset += file.tile_start;  // Skip to data portion of file
  off_t unaligned_amount = offset & (alignment - 1);
  offset -= unaligned_amount;
  char* tbuf = (char*)cpy_buf;

  debug_printf("%zu bytes into %p from offset %zu\n", alignment, buf, offset);
  if ( ( rval = ::pread(file.fd, tbuf, alignment, offset) ) == -1) {
    perror("ERROR: pread failed");
    exit(1);
  }

  ssize_t amount_to_copy = std::min(rval, nbytes) - unaligned_amount;

  std::memcpy(buf, &tbuf[unaligned_amount], amount_to_copy);

  return amount_to_copy;
}

std::ostream &operator<<(std::ostream &os, Tile const &ft)
{
  os << ft.file.fname << " "
     << "Start=" << ft.file.tile_start << ", "
     << "Size=" << ft.file.tile_size << ", "
     << "XDim=" << ft.dim.xDim << ", "
     << "YDim=" << ft.dim.yDim << ", "
     << "ESize=" << ft.dim.elem_size << " ";

  return os;
}
}
