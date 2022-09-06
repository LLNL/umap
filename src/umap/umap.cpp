//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

#include <cinttypes>
#include <errno.h>              // strerror()
#include <string.h>             // strerror()
#include <sys/mman.h>

#include "umap/config.h"

#include "umap/RegionManager.hpp"
#include "umap/umap.h"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

void*
umap(
    void* region_addr
  , uint64_t region_size
  , int prot
  , int flags
  , int fd
  , off_t offset
)
{
  UMAP_LOG(Debug, 
      "region_addr: " << region_addr
      << ", region_size: " << region_size
      << ", prot: " << prot
      << ", flags: " << flags
      << ", offset: " << offset
  );
  return Umap::umap_ex(region_addr, region_size, prot, flags, fd, 0, nullptr);
}

void*
 umap_variable(
     void* region_addr
   , uint64_t region_size
   , int prot
   , int flags
   , int fd
   , off_t offset
   , uint64_t page_size
 )
 {
   UMAP_LOG(Debug, 
      "region_addr: " << region_addr
      << ", region_size: " << region_size
      << ", prot: " << prot
      << ", flags: " << flags
      << ", offset: " << offset
      << ", page_size: " << page_size
  );
  return Umap::umap_ex(region_addr, region_size, prot, flags, fd, 0, nullptr, page_size);
}

void umap_adapt_pagesize(
     void* addr
   , uint64_t page_size
){
  auto& rm = Umap::RegionManager::getInstance();
  rm.adaptRegion((char*)addr, page_size);
}

int
uunmap(void*  addr, uint64_t length)
{
  auto& rm = Umap::RegionManager::getInstance();
  rm.removeRegion((char*)addr);
  return 0;
}


int umap_flush(){  
  auto& rm = Umap::RegionManager::getInstance();
  rm.flush_buffer();
  return 0;
}

int umap_has_write_support(){
#ifdef UMAP_RO_MODE
  return 0;
#else
  return 1;
#endif
}

void umap_prefetch( int npages, umap_prefetch_item* page_array )
{
  Umap::RegionManager::getInstance().prefetch(npages, page_array);
}

void umap_fetch_and_pin( char* paddr, uint64_t size )
{
  Umap::RegionManager::getInstance().fetch_and_pin(paddr, size);
}


long
umapcfg_get_system_page_size( void )
{
  return Umap::RegionManager::getInstance().get_system_page_size();
}

uint64_t
umapcfg_get_max_pages_in_buffer( void )
{
  return Umap::RegionManager::getInstance().get_max_pages_in_buffer();
}

uint64_t
umapcfg_get_umap_page_size( void )
{
  return Umap::RegionManager::getInstance().get_umap_page_size();
}

uint64_t
umapcfg_get_num_fillers( void )
{
  return Umap::RegionManager::getInstance().get_num_fillers();
}

uint64_t
umapcfg_get_num_evictors( void )
{
  return Umap::RegionManager::getInstance().get_num_evictors();
}

int
umapcfg_get_evict_low_water_threshold( void )
{
  return Umap::RegionManager::getInstance().get_evict_low_water_threshold();
}

int
umapcfg_get_evict_high_water_threshold( void )
{
  return Umap::RegionManager::getInstance().get_evict_high_water_threshold();
}

uint64_t
umapcfg_get_max_fault_events( void )
{
  return Umap::RegionManager::getInstance().get_max_fault_events();
}

namespace Umap {
  
  // A global variable to ensure thread-safety
  std::mutex g_mutex;

  void*
  umap_ex(
      void* region_addr
    , uint64_t region_size
    , int prot
    , int flags
    , int fd
    , off_t offset
    , Store* store
    , uint64_t region_page_size
  )
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto& rm = RegionManager::getInstance();
    auto umap_psize = rm.get_umap_page_size();

    if( region_page_size == 0 ){
      region_page_size = umap_psize;
    }else{
      if( region_page_size % umap_psize)
        UMAP_ERROR("region_page_size: " << region_page_size << " should be a multiple of umap_psize " << umap_psize);
    }

  #ifdef UMAP_RO_MODE
    if( prot != PROT_READ )
      UMAP_ERROR("only PROT_READ is supported in UMAP_RO_MODE compilation");
  #else
    if( prot & ~(PROT_READ|PROT_WRITE) )
      UMAP_ERROR("only PROT_READ or PROT_WRITE is supported in UMap");
  #endif
      
    //
    // TODO: Allow for non-page-multiple size and zero-fill like mmap does
    //
    if ( ( region_size % region_page_size ) ) {
      region_size = region_size + region_page_size - (region_size % region_page_size);
    }

    if ( ( (uint64_t)region_addr & (umap_psize - 1) ) ) {
      UMAP_ERROR("region_addr must be page aligned: " << region_addr
        << ", page size is: " << rm.get_umap_page_size());
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
    uint64_t mmap_size = region_size + region_page_size;

    void* mmap_region = mmap(region_addr, mmap_size,
                          prot, flags | (MAP_ANONYMOUS | MAP_NORESERVE), -1, 0);

    if (mmap_region == MAP_FAILED) {
      UMAP_ERROR("mmap failed: " << strerror(errno));
      return UMAP_FAILED;
    }

    uint64_t umap_size = region_size;
    void* umap_region;
    umap_region = (void*)((uint64_t)mmap_region + region_page_size - 1);
    umap_region = (void*)((uint64_t)umap_region & ~(region_page_size - 1));

    if ( store == nullptr )
      store = Store::make_store(umap_region, umap_size, umap_psize, fd);

    rm.addRegion(store, (char*)umap_region, umap_size, (char*)mmap_region, mmap_size, region_page_size);
    UMAP_LOG(Info, 
        "region_addr: " << region_addr
        << ", region_size: " << region_size
        << ", prot: "   << prot
        << ", flags: "  << flags
        << ", offset: " << offset
        << ", store: "  << store
        << ", global_page_size: " << umap_psize
        << ", region_page_size: " << region_page_size
        << ", umap_region_addr: " << umap_region
        << ", umap_region_size: " << umap_size
    );

    return umap_region;
  }
} // namespace Umap
