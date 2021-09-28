//////////////////////////////////////////////////////////////////////////////
//// Copyright 2017-2021 Lawrence Livermore National Security, LLC and other
//// UMAP Project Developers. See the top-level LICENSE file for details.
////
//// SPDX-License-Identifier: LGPL-2.1-only
////////////////////////////////////////////////////////////////////////////////

#include "UmapServiceCommon.hpp"
int memfd_create(const char *name, unsigned int flags) {
  return syscall(SYS_memfd_create, name, flags);
}

unsigned long get_aligned_size(unsigned long fsize, unsigned long page_size){
  return (fsize & (~(page_size - 1))) + page_size;
}

unsigned long get_mmap_size(unsigned long fsize, unsigned long page_size){
  return get_aligned_size(fsize, page_size) + page_size;
}

void *get_umap_aligned_base_addr(void *mmap_addr, uint64_t page_size){
  std::cout<<"Client side page size"<<page_size<<std::endl;
  return (void *)(((unsigned long)mmap_addr + page_size - 1) & (~(page_size - 1)));
}
