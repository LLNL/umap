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

#ifndef UTILITY_FILE_HPP
#define UTILITY_FILE_HPP

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
/// Change file size
/// -------------------------------------------------------------------------------- ///
void extend_file_size_manually(const int fd, const off_t file_size)
{
  unsigned char* buffer = new unsigned char[4096];
  for (off_t i = 0; i < file_size / 4096; ++i) {
    ::pwrite(fd, buffer, 4096, i * 4096);
  }
  const size_t remained_size = file_size % 4096;
  if (remained_size > 0)
    ::pwrite(fd, buffer, remained_size, file_size - remained_size);

  ::sync();
  delete[] buffer;
}

void extend_file_size(const int fd, const size_t file_size)
{
  /// -----  extend the file if its size is smaller than that of mapped area ----- ///
#ifdef __linux__
  struct stat statbuf;
  if (::fstat(fd, &statbuf) == -1) {
    ::perror("fstat");
    return;
  }
  if (::llabs(statbuf.st_size) < static_cast<ssize_t>(file_size)) {
    if (::fallocate(fd, 0, 0, file_size) == -1) {
      ::perror("fallocate");
    }
  }
#else
  std::cout << "Manually extend file size" << std::endl;
  extend_file_size_manually(fd, file_size);
#endif
}

void extend_file_size(const std::string fname, const size_t file_size)
{
  const int fd = ::open(fname.c_str(), O_RDWR);
  if (fd == -1) {
    ::perror("open in extend_file_size");
    return;
  }
  extend_file_size(fd, file_size);
  ::close(fd);
}

void create_file(const std::string fname)
{
  const int fd = ::open(fname.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    ::perror("open in create_file");
    return;
  }
  ::close(fd);
}


/// -------------------------------------------------------------------------------- ///
/// For std::streambuf
/// -------------------------------------------------------------------------------- ///
void set_stream_buffer(std::streambuf *const stream, char *const buffer, const size_t buffer_size)
{
  stream->pubsetbuf(buffer, buffer_size);
}


/// -------------------------------------------------------------------------------- ///
/// Get system status
/// -------------------------------------------------------------------------------- ///
size_t get_num_minor_page_faults()
{
  struct ::rusage usage;
  if (::getrusage(RUSAGE_SELF, &usage) == -1) {
    ::perror("getrusage");
    return 0;
  }
  return usage.ru_minflt;
}

size_t get_num_major_page_faults()
{
  struct ::rusage usage;
  if (::getrusage(RUSAGE_SELF, &usage) == -1) {
    ::perror("getrusage");
    return 0;
  }
  return usage.ru_majflt;
}

size_t get_page_cache_usage()
{
  std::ifstream fin("/proc/meminfo");
  if (!fin.is_open()) {
    std::cout << "Unable to open /proc/meminfo" << std::endl;
    return 0;
  }

  std::string token;
  while (fin >> token) {
    if (token != "Cached:") continue;

    size_t value;
    std::string unit;
    if (fin >> value && fin >> unit) {
      if (unit == "kB" || unit == "KB") {
        return value * 1024;
      }
    }
    std::cout << "Cannot extract information about Cached from /proc/meminfo" << std::endl;
  }

  return 0;
}

}  // namespace utility
#endif // UTILITY_FILE_HPP