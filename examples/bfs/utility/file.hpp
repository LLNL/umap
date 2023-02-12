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
#ifndef LIB_UTILITY_FILE_HPP
#define LIB_UTILITY_FILE_HPP

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <fstream>

namespace utility {
inline void extend_file_size_manually(const int fd, const size_t file_size) {
  auto buffer = new unsigned char[4096];
  for (off_t i = 0; i < file_size / 4096; ++i) {
    ::pwrite(fd, buffer, 4096, i * 4096);
  }
  const size_t remained_size = file_size % 4096;
  if (remained_size > 0)
    ::pwrite(fd, buffer, remained_size, static_cast<off_t>(file_size - remained_size));

  ::sync();
  delete[] buffer;
}

inline bool extend_file_size(const int fd, const size_t file_size) {
  /// -----  extend the file if its size is smaller than that of mapped area ----- ///
#ifdef __linux__
  struct stat statbuf;
  if (::fstat(fd, &statbuf) == -1) {
    ::perror("fstat");
    std::cerr << "errno: " << errno << std::endl;
    return false;
  }
  if (::llabs(statbuf.st_size) < static_cast<ssize_t>(file_size)) {
    if (::ftruncate(fd, file_size) == -1) {
      ::perror("ftruncate");
      std::cerr << "errno: " << errno << std::endl;
      return false;
    }
  }
#else
#warning "Manually extend file size instead of using ftruncate(2)"
  extend_file_size_manually(fd, file_size);
#endif
  return true;
}

inline bool extend_file_size(const std::string &file_name, const size_t file_size) {
  const int fd = ::open(file_name.c_str(), O_RDWR);
  if (fd == -1) {
    ::perror("open");
    std::cerr << "errno: " << errno << std::endl;
    return false;
  }

  if (!extend_file_size(fd, file_size)) return false;
  ::close(fd);

  return true;
}

inline bool create_file(const std::string &file_name) {
  const int fd = ::open(file_name.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    ::perror("open");
    std::cerr << "errno: " << errno << std::endl;
    return false;
  }
  ::close(fd);

  return true;
}

inline ssize_t get_file_size(const std::string &file_name) {
  std::ifstream ifs(file_name, std::ifstream::binary | std::ifstream::ate);
  ssize_t size = ifs.tellg();
  if (size == -1) {
    std::cerr << "Failed to get file size: " << file_name << std::endl;
  }

  return size;
}
}  // namespace utility

#endif //LIB_UTILITY_FILE_HPP
