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
#ifndef SIMPLE_BFS_MMAP_UTILITY_HPP
#define SIMPLE_BFS_MMAP_UTILITY_HPP

#include <sys/mman.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <cstddef>

#include "file.hpp"

namespace utility {

/// \brief Get the page size
/// \return The size of page size. Return -1 for error cases.
inline ssize_t get_page_size() {
  const ssize_t page_size = ::sysconf(_SC_PAGE_SIZE);
  if (page_size == -1) {
    ::perror("sysconf(_SC_PAGE_SIZE)");
    std::cerr << "errno: " << errno << std::endl;
  }

  return page_size;
}

/// \brief Maps a file checking errors
/// \param addr Same as mmap(2)
/// \param length Same as mmap(2)
/// \param protection Same as mmap(2)
/// \param flags Same as mmap(2)
/// \param fd Same as mmap(2)
/// \param offset  Same as mmap(2)
/// \return On success, returns a pointer to the mapped area.
/// On error, nullptr is returned.
inline void *map_file(void *const addr, const size_t length, const int protection, const int flags,
               const int fd, const off_t offset) {
  const ssize_t page_size = get_page_size();
  if (page_size == -1) {
    return nullptr;
  }

  if ((std::ptrdiff_t)addr % page_size != 0) {
    std::cerr << "address (" << addr << ") is not page aligned ("
              << ::sysconf(_SC_PAGE_SIZE) << ")" << std::endl;
    return nullptr;
  }

  if (offset % page_size != 0) {
    std::cerr << "offset (" << offset << ") is not a multiple of the page size (" << ::sysconf(_SC_PAGE_SIZE) << ")"
              << std::endl;
    return nullptr;
  }

  /// ----- Map the file ----- ///
  void *mapped_addr = ::mmap(addr, length, protection, flags, fd, offset);
  if (mapped_addr == MAP_FAILED) {
    ::perror("mmap");
    std::cerr << "errno: " << errno << std::endl;
    return nullptr;
  }

  if ((std::ptrdiff_t)mapped_addr % page_size != 0) {
    std::cerr << "mapped address (" << mapped_addr << ") is not page aligned ("
              << ::sysconf(_SC_PAGE_SIZE) << ")" << std::endl;
    return nullptr;
  }

  return mapped_addr;
}

/// \brief Maps a file with read mode
/// \param file_name The name of file to be mapped
/// \param addr Normally nullptr; if this is not nullptr the kernel takes it as a hint about where to place the mapping
/// \param length The length of the map
/// \param offset The offset in the file
/// \return On Success, returns a pair of the file descriptor of the file and the starting address for the map.
/// On error, returns a pair of -1 and nullptr.
inline std::pair<int, void *> map_file_read_mode(const std::string &file_name, void *const addr,
                                          const size_t length, const off_t offset,
                                          const int additional_flags = 0) {
  /// ----- Open the file ----- ///
  const int fd = ::open(file_name.c_str(), O_RDONLY);
  if (fd == -1) {
    ::perror("open");
    std::cerr << "errno: " << errno << std::endl;
    return std::make_pair(-1, nullptr);
  }

  /// ----- Map the file ----- ///
  void *mapped_addr = map_file(addr, length, PROT_READ, MAP_SHARED | additional_flags, fd, offset);
  if (mapped_addr == nullptr) {
    close(fd);
    return std::make_pair(-1, nullptr);
  }

  return std::make_pair(fd, mapped_addr);
}

/// \brief Maps a file with write mode
/// \param file_name The name of a file to be mapped
/// \param addr Normally nullptr; if this is not nullptr the kernel takes it as a hint about where to place the mapping
/// \param length The length of the map
/// \param offset The offset in the file
/// \return On Success, returns a pair of the file descriptor of the file and the starting address for the map.
/// On error, returns a pair of -1 and nullptr.
inline std::pair<int, void *> map_file_write_mode(const std::string &file_name, void *const addr,
                                           const size_t length, const off_t offset,
                                           const int additional_flags = 0) {
  /// ----- Open the file ----- ///
  const int fd = ::open(file_name.c_str(), O_RDWR);
  if (fd == -1) {
    ::perror("open");
    std::cerr << "errno: " << errno << std::endl;
    return std::make_pair(-1, nullptr);
  }

  /// ----- Map the file ----- ///
  void *mapped_addr = map_file(addr, length, PROT_READ | PROT_WRITE, MAP_SHARED | additional_flags, fd, offset);
  if (mapped_addr == nullptr) {
    close(fd);
    return std::make_pair(-1, nullptr);
  }

  return std::make_pair(fd, mapped_addr);
}

inline void msync(void *const addr, const size_t length) {
  if (::msync(addr, length, MS_SYNC) != 0) {
    ::perror("msync");
    std::cerr << "errno: " << errno << std::endl;
  }
}

inline void munmap(void *const addr, const size_t length, const bool call_msync) {
  if (call_msync) msync(addr, length);

  if (::munmap(addr, length) != 0) {
    ::perror("munmap");
    std::cerr << "errno: " << errno << std::endl;
  }
}

/// \brief Returns the number of page faults caused by the process
/// \return A pair of #of minor and major page faults
inline std::pair<std::size_t, std::size_t> get_num_page_faults()
{
  std::size_t minflt = 0;
  std::size_t majflt = 0;
#ifdef __linux__
  const char* stat_path = "/proc/self/stat";
  FILE *f = ::fopen(stat_path, "r");
  if (f) {
    // 0:pid 1:comm 2:state 3:ppid 4:pgrp 5:session 6:tty_nr 7:tpgid 8:flags 9:minflt 10:cminflt 11:majflt
    int ret;
    if ((ret = ::fscanf(f,"%*d %*s %*c %*d %*d %*d %*d %*d %*u %lu %*u %lu", &minflt, &majflt)) != 2) {
      std::cerr << "Failed to reading #of page faults " << ret << std::endl;
      minflt = majflt = 0;
    }
  }
  fclose(f);
#else
#warning "get_num_page_faults() is not supported in this environment"
#endif
  return std::make_pair(minflt, majflt);
}
} // namespace utility

#endif //SIMPLE_BFS_MMAP_UTILITY_HPP
