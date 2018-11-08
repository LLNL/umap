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
#include <unistd.h>
#include <stdio.h>
#include "umap/Store.h"
#include "StoreFile.h"

using namespace std;

StoreFile::StoreFile(void* _region_, size_t _rsize_, size_t _alignsize_, int _fd_)
  : region{_region_}, rsize{_rsize_}, alignsize{_alignsize_}, fd{_fd_}
{
}

ssize_t StoreFile::read_from_store(char* buf, size_t nb, off_t off)
{
  ssize_t rval;

  if ( ( rval = pread(fd, buf, nb, off) ) == -1) {
    perror("ERROR: pread failed");
    _exit(1);
  }
  return rval;
}

ssize_t  StoreFile::write_to_store(char* buf, size_t nb, off_t off)
{
  ssize_t rval;

  if ( ( rval = pwrite(fd, buf, nb, off) ) == -1) {
    perror("ERROR: pwrite failed");
    _exit(1);
  }
  return rval;
}
