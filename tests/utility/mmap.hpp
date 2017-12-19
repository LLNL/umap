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

#ifndef UTILITY_MMAP_HPP
#define UTILITY_MMAP_HPP

#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "file.hpp"

namespace utility
{

void* map_file(void *const addr, const size_t length, const int protection, const int flags,
                               const int fd, const off_t offset)
{
  const size_t page_size = ::sysconf(_SC_PAGE_SIZE);
  if (offset % page_size != 0) {
    std::cout << "offset (" << offset << ") is not a multiple of the page size ("<< sysconf(_SC_PAGE_SIZE) << ")" << std::endl;
    return MAP_FAILED;
  }

  /// ----- Map the file ----- ///
  void* mapped_addr = reinterpret_cast<unsigned char *>(::mmap(addr, length, protection, flags, fd, offset));
  if (mapped_addr == MAP_FAILED) {
    ::perror("mmap");
    return MAP_FAILED;
  }

  return mapped_addr;
}

/// \brief map a file with read mode
/// \param file_name the name of file to be mapped
/// \param addr normaly nullptr; if this is not nullptr the kernel takes it as a hint about where to place the mapping
/// \param length the lenght of the map
/// \param offset the offset in the file
/// \return a pair of the file descriptor of the file and the starting address for the map
std::pair<int, void*> map_file_read_mode(const std::string& file_name, void *const addr, const size_t length, const off_t offset)
{
  /// ----- Open the file ----- ///
  const int fd = open(file_name.c_str(), O_RDONLY);
  if (fd == -1) { ::perror("open"); return std::make_pair(-1, nullptr); }

  /// ----- Map the file ----- ///
  void* mapped_addr = reinterpret_cast<unsigned char *>(map_file(addr, length, PROT_READ, MAP_PRIVATE, fd, offset));
  if (mapped_addr == MAP_FAILED) {
    close(fd);
    return std::make_pair(-1, nullptr);
  }

  return std::make_pair(fd, mapped_addr);
}

/// \brief map a file with write mode
/// \param file_name the name of file to be mapped
/// \param addr normaly nullptr; if this is not nullptr the kernel takes it as a hint about where to place the mapping
/// \param length the lenght of the map
/// \param offset the offset in the file
/// \return a pair of the file descriptor of the file and the starting address for the map
std::pair<int, void*> map_file_write_mode(const std::string& file_name, void *const addr, const size_t length, const off_t offset)
{
  /// ----- Open the file ----- ///
  const int fd = open(file_name.c_str(), O_RDWR);
  if (fd == -1) { ::perror("open"); return std::make_pair(-1, nullptr); }

  /// ----- Map the file ----- ///
  void* mapped_addr = reinterpret_cast<unsigned char *>(map_file(addr, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset));
  if (mapped_addr == MAP_FAILED) {
    close(fd);
    return std::make_pair(-1, nullptr);
  }

  return std::make_pair(fd, mapped_addr);
}

/// \brief create and map a file with write mode
/// \param file_name the name of file to be mapped
/// \param addr normaly nullptr; if this is not nullptr the kernel takes it as a hint about where to place the mapping
/// \param length the length of the map in byte
/// \param offset the offset in the file
/// \return a pair of the file descriptor of the file and the starting address for the map
std::pair<int, void*> create_and_map_file_write_mode(const std::string& file_name, void *const addr, const size_t length, const size_t offset)
{

  create_file(file_name);
  extend_file_size(file_name, length);

  return map_file_write_mode(file_name, addr, length, offset);
}

void msync(void *const addr, const size_t length)
{
  ::msync(addr, length, MS_SYNC);
}

void munmap(const int fd, void *const addr, const size_t length)
{
  msync(addr, length);

  if (::munmap(addr, length) == -1) {
    ::perror("munmap");
  }

  ::close(fd);
}

void madvise_random(void *const addr, const size_t length)
{
//  std::cout << "madvise MADV_RANDOM" << std::endl;

  const int ret = ::madvise(addr, length, MADV_RANDOM);
  if (ret != 0) { ::perror("madvise MADV_RANDOM"); }
}

void madvise_dontneed(void *const addr, const size_t length)
{
//  std::cout << "madvise MADV_DONTNEED" << std::endl;

  const int ret = ::madvise(addr, length, MADV_DONTNEED);
  if (ret != 0) { ::perror("madvise MADV_DONTNEED"); }
}

void madvise_willneed(void *const addr, const size_t length)
{
//  std::cout << "madvise MADV_WILLNEED" << std::endl;

  const int ret = ::madvise(addr, length, MADV_WILLNEED);
  if (ret != 0) { ::perror("madvise MADV_WILLNEED"); }
}


} // namespace utility
#endif // UTILITY_MMAP_HPP
