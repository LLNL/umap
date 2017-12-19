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
#ifndef UTILITY_MEMORY_HPP
#define UTILITY_MEMORY_HPP

#include <iostream>
#include <fstream>

#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "common.hpp"

namespace utility
{

enum alloc_page_size {
  huge_page,
  normal_page
};

/// TODO: get this value from system (/proc/meminfo?)
static const size_t kBit_shift_length_page_size = 21;


/// \brief NOTE: In LC system, you have to use reserve huge pages before running this program
/// using salloc or srun's --hugepage option
/// \param size
/// \return
void *malloc_hugepage(const size_t size)
{
#ifdef __linux__
  /// TODO: implement this to be able to specify page_size
  //  const size_t page_size = log_page_size << MAP_HUGE_SHIFT;

  /// TODO: disable huge page sized allocation if there is not enough amount of memory is allocated
  void *addr = mmap(NULL,
                    size,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB/* | page_size*/,
                    -1,
                    0);
#else
  std::cout << "Huge Page is not supported" << std::endl;
  void *addr = mmap(NULL,
                    size,
                    PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                    -1,
                    0);
#endif

  if (addr == MAP_FAILED) {
    perror("mmap");
    return nullptr;
  }

  return addr;
}


void* malloc(const size_t size, const alloc_page_size psize = alloc_page_size::normal_page)
{
  if (psize == alloc_page_size::huge_page) {
    return malloc_hugepage(size);
  } else {
    void *addr = ::malloc(size);
    if (addr == NULL && size > 0) {
      perror("malloc");
      return nullptr;
    }
    return addr;
  }

  return nullptr;
}

void* aligned_malloc(const size_t alignment, const size_t size)
{
  if (!is_power_of_2(alignment)) {
    std::cout << "Invalid argument in " << __func__ << ": given 'alignment' is not power of 2 " << alignment << std::endl;
    return nullptr;
  }

  void *addr = nullptr;
  const int error = ::posix_memalign(&addr, alignment, size);

  if (error == EINVAL) {
    std::cout << "posix_memalign returned an error: EINVAL" << std::endl;
    return nullptr;
  } else if (error == ENOMEM) {
    std::cout << "posix_memalign returned an error: ENOMEM" << std::endl;
    return nullptr;
  } else if (error > 0) {
    std::cout << "posix_memalign returned an unknown error" << std::endl;
    return nullptr;
  } else if (addr == nullptr) {
    std::cout << "posix_memalign returned nullptremacs " << std::endl;
    return nullptr;
  }

  return addr;
}

void free(void *const addr)
{
  ::free(addr);
}

void free(void *const addr, size_t size, const alloc_page_size psize = alloc_page_size::normal_page)
{
  if (psize == alloc_page_size::huge_page) {
#ifdef __linux__
    /// For mappings that employ huge pages,
    /// length must both be a multiple of the underlying huge page size
    size = roundup(size, (1ULL << kBit_shift_length_page_size));
#endif
    if (::munmap(addr, size) != 0) { perror("munmap"); }
  } else if (psize == alloc_page_size::normal_page) {
    ::free(addr);
  } else {
    std::cout << "Wrong alloc_page_size is given" << std::endl;
  }
}

size_t get_allocated_page_size(const void *const addr)
{

  std::ifstream file("/proc/self/smaps");
  if (!file.is_open()) {
    std::cout << "Unable to open /proc/self/smaps" << std::endl;
    return 0;
  }

  while(!file.good()) {
    void* addr_start;
    void* addr_end;

    /// --- Find target block using address --- ///
    std::string line;
    std::getline(file,line);
    char dummy[128];
    const int ret = ::sscanf(line.c_str(),
                           "%p-%p %s %s %s %s %s",
                           &addr_start, &addr_end, dummy, dummy, dummy, dummy, dummy);
    if (ret < 6) continue; /// Read different line
    if (addr < addr_start || addr_end <= addr) continue; /// Address is different

    std::string token;
    while (file >> token) {
      if (token != "KernelPageSize:") continue;

      size_t page_size;
      std::string unit;
      if (file >> page_size && file >> unit) {
        if (unit == "kB" || unit == "KB") {
          return page_size * 1024;
        }
      }
      std::cout << "Cannot get information about KernelPageSize from /proc/self/smaps" << std::endl;
    }
  }

  return 0;
}


size_t get_system_ram_size()
{
  return ::sysconf(_SC_PHYS_PAGES) * ::sysconf(_SC_PAGE_SIZE);
}

} // namespace utility
#endif // UTILITY_MEMORY_HPP
