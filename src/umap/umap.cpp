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
  return Umap::umap_ex(region_addr, region_size, prot, flags, fd, 0, nullptr, false, 0);
}

int
uunmap(void*  addr, uint64_t length)
{
  UMAP_LOG(Debug, "addr: " << addr << ", length: " << length);
  auto& rm = Umap::RegionManager::getInstance();
  rm.removeRegion((char*)addr);
  UMAP_LOG(Debug, "Done");
  return 0;
}

//Todo: Might replace this implementation with a call to a function within Umap namespace

int umap_flush(){
  
  UMAP_LOG(Debug,  "umap_flush " );
  
  return Umap::RegionManager::getInstance().flush_buffer();

}


void umap_prefetch( int npages, umap_prefetch_item* page_array, int client_fd/*default=0*/)
{ 
  Umap::RegionManager::getInstance().prefetch(npages, page_array, client_fd);
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
umapcfg_get_read_ahead( void )
{
  return Umap::RegionManager::getInstance().get_read_ahead();
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

void 
terminate_handler(int client_fd)
{
  UMAP_LOG(Info, "terminating handler for client_fd: " << client_fd);
  auto& rm = Umap::RegionManager::getInstance();
  rm.terminateUffdHandler(client_fd);
  UMAP_LOG(Info, "Done");
}

bool
uunmap_server(void *addr, uint64_t length, int client_fd, int file_fd, bool client_term){
  UMAP_LOG(Debug, "addr: " << addr << ", length: " << length);
  auto& rm = Umap::RegionManager::getInstance();
  bool ret = rm.removeRegion((char*)addr, client_fd, file_fd, client_term);
  UMAP_LOG(Debug, "Done");
  return ret; 
}

void*
umap_ex(
    void* region_addr
  , uint64_t region_size
  , int prot
  , int flags
  , int fd
  , off_t offset
  , Store* store
  , bool server
  , int client_uffd
  , void* remote_base
)
{
  std::lock_guard<std::mutex> lock(g_mutex);
  void* umap_region = NULL;
  uint64_t umap_size = 0;
  uint64_t mmap_size = 0;
  void* mmap_region = 0;
  void *reg_desc = NULL;
 
  auto& rm = RegionManager::getInstance();
  auto umap_psize = rm.get_umap_page_size();
  //check if the file has already been mapped before

  UMAP_LOG(Info, 
      "region_addr: " << region_addr
      << ", region_size: " << region_size
      << ", prot: " << prot
      << ", flags: " << flags
      << ", offset: " << offset
      << ", store: " << store
      << ", umap_psize: " << umap_psize
  );

#ifdef UMAP_RO_MODE
  if( !server && prot != PROT_READ )
    UMAP_ERROR("only PROT_READ is supported in UMAP_RO_MODE compilation");
#else
  if( prot & ~(PROT_READ|PROT_WRITE) )
    UMAP_ERROR("only PROT_READ or PROT_WRITE is supported in UMap");
#endif
    
  //
  // TODO: Allow for non-page-multiple size and zero-fill like mmap does
  //
  if ( ( region_size % umap_psize ) ) {
    UMAP_ERROR("Region size " << region_size 
                << " is not a multple of umapPageSize (" 
                << rm.get_umap_page_size() << ")");
  }

  if ( ( (uint64_t)region_addr & (umap_psize - 1) ) ) {
     UMAP_ERROR("region_addr must be page aligned: " << region_addr
      << ", page size is: " << rm.get_umap_page_size());
  }

  if(!server){
    if (!(flags & UMAP_PRIVATE) || flags & ~(UMAP_PRIVATE|UMAP_FIXED)) {
      UMAP_ERROR("Invalid flags: " << std::hex << flags);
    }
  }

  reg_desc = rm.isFDRegionPresent(fd);
  
  if(reg_desc==NULL){
    //
    // When dealing with umap-page-sizes that could be multiples of the actual
    // system-page-size, it is possible for mmap() to provide a region that is on
    // a system-page-boundary, but not necessarily on a umap-page-size boundary.
    //
    // We always allocate an additional umap-page-size set of bytes so that we can
    // make certain that the umap-region begins on a umap-page-size boundary.
    //
    mmap_size = region_size + umap_psize;

    if(!server){
      mmap_region = mmap(region_addr, mmap_size,
                       prot, flags | (MAP_ANONYMOUS | MAP_NORESERVE), -1, 0);

      if (mmap_region == MAP_FAILED) {
        UMAP_ERROR("mmap failed: " << strerror(errno));
        return UMAP_FAILED;
      }
    }else{
      mmap_region = region_addr;
    }
    umap_size = region_size;
    umap_region = (void*)((uint64_t)mmap_region + umap_psize - 1);
    umap_region = (void*)((uint64_t)umap_region & ~(umap_psize - 1));

    if ( store == nullptr )
      store = Store::make_store(umap_region, umap_size, umap_psize, fd);
    rm.addRegion(fd, store, umap_region, umap_size, (char*)mmap_region, mmap_size, server, client_uffd, remote_base);
  }else{
    umap_region = (void *)rm.associateRegion(fd, reg_desc, server, client_uffd,remote_base);
  }

  return umap_region;
}
} // namespace Umap
