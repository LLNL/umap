//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

#include <cinttypes>
#include <errno.h>              // strerror()
#include <string.h>             // strerror()
#include <sys/mman.h>

#include "umap/config.h"

#include "umap/PageRegion.hpp"
#include "umap/store/Store.hpp"
#include "umap/umap.h"
#include "umap/util/Macros.hpp"

void* umap(
    void* base_addr
  , uint64_t region_size
  , int prot
  , int flags
  , int fd
  , off_t offset
)
{
  return Umap::umap_ex(base_addr, region_size, prot, flags, fd, 0, nullptr);
}

namespace Umap {
void* umap_ex(
    void* base_addr
  , uint64_t region_size
  , int prot
  , int flags
  , int fd
  , off_t offset
  , Store* store
)
{
  auto fm = PageRegion::getInstance();
  auto umap_psize = fm->get_umap_page_size();

  if ( ( region_size % umap_psize ) ) {
    UMAP_ERROR("Region size " << region_size 
                << " is not a multple of umapPageSize (" 
                << fm->get_umap_page_size() << ")");
  }

  if ( ( (uint64_t)base_addr & (umap_psize - 1) ) ) {
    UMAP_ERROR("base_addr must be page aligned: " << base_addr
      << ", page size is: " << fm->get_umap_page_size());
  }

  if (!(flags & UMAP_PRIVATE) || flags & ~(UMAP_PRIVATE|UMAP_FIXED)) {
    UMAP_ERROR("Invalid flags: " << std::hex << flags);
  }

  //
  // When dealing with umap-page-sizes that could be multiples of the actual
  // system-page-size, it is possible for mmap() to provide a region that is on
  // a system-page-boundary, but not necessarily on a umap-page-size boundary.
  //
  // We always allocate an additional umap-page-size set of bytes so that we can
  // make certain that the umap-region begins on a umap-page-size boundary.
  //
  uint64_t mmap_size = region_size + umap_psize;

  void* mmap_region = mmap(base_addr, mmap_size,
                        prot, flags | (MAP_ANONYMOUS | MAP_NORESERVE), -1, 0);

  if (mmap_region == MAP_FAILED) {
    UMAP_ERROR("mmap failed: " << strerror(errno));
    return UMAP_FAILED;
  }
  uint64_t umap_size = region_size;
  void* umap_region;
  umap_region = (void*)((uint64_t)mmap_region + umap_psize - 1);
  umap_region = (void*)((uint64_t)umap_region & ~(umap_psize - 1));

  if ( store == nullptr )
    store = Store::make_store(umap_region, umap_size, umap_psize, fd);

  fm->makePageFiller(store, (char*)umap_region, umap_size, (char*)mmap_region, mmap_size);

  return umap_region;
}
} // namespace Umap

int uunmap(void*  addr, uint64_t length)
{
  sleep(30);
  auto fm = Umap::PageRegion::getInstance();
  fm->destroyPageFiller((char*)addr);
  return 0;
}

long     umapcfg_get_system_page_size( void )
{
  return Umap::PageRegion::getInstance()->get_system_page_size();
}

uint64_t umapcfg_get_max_pages_in_buffer( void )
{
  return Umap::PageRegion::getInstance()->get_max_pages_in_buffer();
}

void     umapcfg_set_max_pages_in_buffer( uint64_t max_pages )
{
  Umap::PageRegion::getInstance()->set_max_pages_in_buffer(max_pages);
}

uint64_t umapcfg_get_umap_page_size( void )
{
  return Umap::PageRegion::getInstance()->get_umap_page_size();
}
void     umapcfg_set_umap_page_size( uint64_t page_size )
{
  Umap::PageRegion::getInstance()->set_umap_page_size(page_size);
}

uint64_t umapcfg_get_num_fillers( void )
{
  return Umap::PageRegion::getInstance()->get_num_fillers();
}
void     umapcfg_set_num_fillers( uint64_t num_fillers )
{
  Umap::PageRegion::getInstance()->set_num_fillers(num_fillers);
}

uint64_t umapcfg_get_num_flushers( void )
{
  return Umap::PageRegion::getInstance()->get_num_flushers();
}
void     umapcfg_set_num_flushers( uint64_t num_flushers )
{
  Umap::PageRegion::getInstance()->set_num_flushers(num_flushers);
}

uint64_t umapcfg_get_max_fault_events( void )
{
  return Umap::PageRegion::getInstance()->get_max_fault_events();
}
void     umapcfg_set_max_fault_events( uint64_t max_events )
{
  Umap::PageRegion::getInstance()->set_max_fault_events(max_events);
}
