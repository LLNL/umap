/*
This file is part of UMAP.  For copyright information see the COPYRIGHT
file in the top level directory, or at
https://github.com/LLNL/umap/blob/master/COPYRIGHT
This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free
Software Foundation) version 2.1 dated February 1999.  This program is
distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the terms and conditions of the GNU Lesser General Public License
for more details.  You should have received a copy of the GNU Lesser General
Public License along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/


#ifndef UTILITY_DIRECT_IO_HPP
#define UTILITY_DIRECT_IO_HPP

#include <iostream>
#include <string>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>

#include "common.hpp"
#include "memory.hpp"

namespace utility
{

/// -------------------------------------------------------------------------------- ///
/// Direct I/O
/// -------------------------------------------------------------------------------- ///
bool load_file_with_direct_io(std::string fname, unsigned char *const buffer, const ssize_t count)
{

  const ssize_t io_chunk_size = 1ULL << 28;
  const size_t storage_block_size = 512; /// In 3D XPoint, it is 4096?

#ifdef O_DIRECT
  const int flags = O_RDONLY | O_DIRECT;
#else
  const int flags = O_RDONLY;
  std::cout << "O_DIRECT is not supported!\n";
  std::cout << "will use normal I/O\n";
#endif

  const int fd = ::open(fname.c_str(), flags);
  if (fd == -1) {
    perror("open");
    return false;
  }

  unsigned char *const internal_buffer = reinterpret_cast<unsigned char *>(utility::aligned_malloc(storage_block_size,
                                                                                                   io_chunk_size));
  if (internal_buffer == nullptr) {
    std::cout << "Failed to allocate an internal buffer" << std::endl;
    return false;
  }

  ssize_t total_read_count = 0;
  while (total_read_count < count) {

    ssize_t request_count = 0;
    if (total_read_count + io_chunk_size <= count) request_count = io_chunk_size;
    else request_count = count - total_read_count;

    const ssize_t aligned_count = utility::roundup(request_count, storage_block_size);
    if (aligned_count > io_chunk_size) {
      std::cout << "Error : aligned_count > io_chunk_size" << std::endl;
      return false;
    }
    const ssize_t read_count = ::read(fd, internal_buffer, aligned_count);

    if (read_count == -1) {
      perror("read");
      return false;
    }
    if (read_count < request_count) {
      std::cout << "Error " << read_count << " / " << request_count << " is read" << std::endl;
      return false;
    }

    std::copy(&internal_buffer[0], &internal_buffer[read_count], &buffer[total_read_count]);

    total_read_count += read_count;
  }

  ::close(fd);
  utility::free(internal_buffer, io_chunk_size);

  return true;
}

}  // namespace utility
#endif //UTILITY_DIRECT_IO_HPP
