//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include "umap/config.h"

#include <cstdint>        // uint64_t
#include <fstream>        // for reading meminfo
#include <mutex>
#include <stdlib.h>       // getenv()
#include <sstream>        // string to integer operations
#include <string>         // string to integer operations
#include <thread>         // for max_concurrency
#include <unordered_map>
#include <unistd.h>       // sysconf()

#include "umap/Buffer.hpp"
#include "umap/EvictManager.hpp"
#include "umap/FillWorkers.hpp"
#include "umap/RegionManager.hpp"
#include "umap/RegionDescriptor.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {

RegionManager&
RegionManager::getInstance( void )
{
  static RegionManager region_manager_instance;

  return region_manager_instance;
}

void *
RegionManager::isFDRegionPresent(int filefd){
  auto it = m_fd_rd_map.find(filefd);
  if(it == m_fd_rd_map.end()){
    return NULL;
  }else{
    return (void *)(it->second);
  }
}

void
RegionManager::setFDRegionMap(int file_fd, RegionDescriptor *rd){
  m_fd_rd_map[file_fd] = rd;
}

char *
RegionManager::associateRegion(int fd, void* existing_rd, bool server, int client_fd, void *remote_base){
  Uffd *c_uffd;
  auto rd = (RegionDescriptor *)existing_rd;
  c_uffd = getActiveUffd(server, client_fd);
  //registering the region has to use remote address
  UMAP_LOG(Debug, "Associating region "<<remote_base);
  c_uffd->register_region(rd, remote_base);
  return rd->start();
}

Uffd*
RegionManager::getActiveUffd(bool server, int client_fd){
  auto it = m_client_uffds.find(client_fd);
  Uffd* c_uffd;
  if(it == m_client_uffds.end()){
    UMAP_LOG(Debug, "New region, new uffd");
    c_uffd = new Uffd(server, client_fd);
    m_client_uffds[client_fd] = c_uffd; 
  }else{
    c_uffd = it->second;
  }
  return c_uffd;
}

void
RegionManager::addRegion(int fd, Store* store, void* region, uint64_t region_size, char* mmap_region, uint64_t mmap_region_size, bool server, int client_fd, void *remote_base)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  Uffd *c_uffd = NULL;

  if ( m_active_regions.empty() ) {
    UMAP_LOG(Debug, "No active regions, initializing engine");
    m_buffer = new Buffer();
    c_uffd = getActiveUffd(server, client_fd);
    m_fill_workers = new FillWorkers();
    m_evict_manager = new EvictManager();
  }
  if(!c_uffd){
    c_uffd = getActiveUffd(server, client_fd);
  }

  //Lookup if the region already exists, i.e. the file has a map already
  auto rd = new RegionDescriptor((char *)region, region_size, mmap_region, mmap_region_size, store);
  m_active_regions[(void*)region] = rd;
  setFDRegionMap(fd, rd);

  UMAP_LOG(Info,
      "Adding region: " << (void*)(rd->start()) << " - " << (void*)(rd->end())
      << ", region_size: " << rd->size()
      << ", number of regions: " << m_active_regions.size() + 1
  );

  //Registering the region needs to use the remote address
  c_uffd->register_region(rd, remote_base);
  m_last_iter = m_active_regions.end();
}

//Assumption here is that all the regions registered by the 
//Server should have been removed by now before calling this function
//The only time the Uffd can't be found is when all the regions got
//removed 
void 
RegionManager::terminateUffdHandler(int client_fd)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  auto uit = m_client_uffds.find(client_fd);
  if(uit == m_client_uffds.end()){
    UMAP_LOG(Info, "Can't find uffd for client fd: " << client_fd);
    return;
  }

  Uffd* c_uffd = uit->second;
  m_client_uffds.erase(uit);
  delete c_uffd; c_uffd = nullptr;
  
  if(m_client_uffds.empty()){
    delete m_evict_manager; m_evict_manager = nullptr;
    delete m_fill_workers; m_fill_workers = nullptr;
    m_client_uffds.erase(uit);
    delete c_uffd; c_uffd = nullptr;
    delete m_buffer; m_buffer = nullptr;
  }
}

bool
RegionManager::removeRegion( char* region, int client_fd, int filefd, bool client_term) 
{
  std::lock_guard<std::mutex> lock(m_mutex);
  auto it = m_active_regions.find(region);
  auto uit = m_client_uffds.find(client_fd);
  bool ret = false;

  if(uit == m_client_uffds.end()){
    UMAP_ERROR("Can't find uffd for client fd: " << client_fd);
  }

  Uffd* c_uffd = uit->second;

  if (it == m_active_regions.end())
    UMAP_ERROR("umap fault monitor not found for: " << (void*)region);

  UMAP_LOG(Info,
      "region: " << (void*)(it->second->start()) << " - " << (void*)(it->second->end())
      << ", region_size: " << it->second->size()
      << ", number of regions: " << m_active_regions.size()
  );
  
  auto rd = it->second;
  c_uffd->unregister_region(rd, client_term);

  if(rd->can_release()){
    UMAP_LOG(Info, "1. releasing the region:  " << (void*)region);
    c_uffd->release_buffer(rd);
    delete rd;
    m_active_regions.erase(it);
    if(client_fd){
      auto fit = m_fd_rd_map.find(filefd);
      if(fit != m_fd_rd_map.end()){
        m_fd_rd_map.erase(fit);
      }
    }
    ret = true;
  }
  m_last_iter = m_active_regions.end();
  return ret;
#if 0
  if ( m_active_regions.empty() ) {
    delete m_evict_manager; m_evict_manager = nullptr;
    delete m_fill_workers; m_fill_workers = nullptr;
    m_client_uffds.erase(uit);
    delete c_uffd; c_uffd = nullptr;
    delete m_buffer; m_buffer = nullptr;
  }
#endif
}

int 
RegionManager::flush_buffer(){

  std::lock_guard<std::mutex> lock(m_mutex);

  m_buffer->flush_dirty_pages();

  return 0;
}

void
RegionManager::prefetch(int npages, umap_prefetch_item* page_array, int client_fd)
{
  Uffd *c_uffd;
  auto it=m_client_uffds.find(client_fd);
  if(it==m_client_uffds.end()){
    c_uffd = it->second;
  }else{
    UMAP_ERROR("client UFFD not found for client_fd: " << client_fd); 
  } 
  for (int i{0}; i < npages; ++i)
    c_uffd->process_page(false, (char*)(page_array[i].page_base_addr));
}

RegionManager::RegionManager()
{
  m_version.major = UMAP_VERSION_MAJOR;
  m_version.minor = UMAP_VERSION_MINOR;
  m_version.patch = UMAP_VERSION_PATCH;

  m_last_iter = m_active_regions.end();

  m_system_page_size = sysconf(_SC_PAGESIZE);

  const uint64_t MAX_FAULT_EVENTS = 256;
  uint64_t env_value = 0;
  if ( (read_env_var("UMAP_MAX_FAULT_EVENTS", &env_value)) != nullptr )
    set_max_fault_events(env_value);
  else
    set_max_fault_events(MAX_FAULT_EVENTS);

  unsigned int nthreads = std::thread::hardware_concurrency();
  nthreads = (nthreads == 0) ? 16 : nthreads;

  if ( (read_env_var("UMAP_PAGE_FILLERS", &env_value)) != nullptr )
    set_num_fillers(env_value);
  else
    set_num_fillers(nthreads);

  if ( (read_env_var("UMAP_PAGE_EVICTORS", &env_value)) != nullptr )
    set_num_evictors(env_value);
  else
    set_num_evictors(nthreads);

  if ( (read_env_var("UMAP_EVICT_HIGH_WATER_THRESHOLD", &env_value)) != nullptr )
    set_evict_high_water_threshold(env_value);
  else
    set_evict_high_water_threshold(90);

  if ( (read_env_var("UMAP_EVICT_LOW_WATER_THRESHOLD", &env_value)) != nullptr )
    set_evict_low_water_threshold(env_value);
  else
    set_evict_low_water_threshold(70);

  if ( (read_env_var("UMAP_PAGESIZE", &env_value)) != nullptr )
    set_umap_page_size(env_value);
  else
    set_umap_page_size(m_system_page_size);

  if ( (read_env_var("UMAP_BUFSIZE", &env_value)) != nullptr )
    set_max_pages_in_buffer(env_value);
  else
    set_max_pages_in_buffer( get_max_pages_in_memory() );

  if ( (read_env_var("UMAP_READ_AHEAD", &env_value)) != nullptr )
    set_read_ahead(env_value);
  else
    set_read_ahead(0);
}

uint64_t
RegionManager::get_max_pages_in_memory( void )
{
  static uint64_t total_mem_kb = 0;
  const uint64_t oneK = 1024;
  const uint64_t percent = 90;  // 90% of available memory

  // Lazily set total_mem_kb global
  if ( ! total_mem_kb ) {
    std::string token;
    std::ifstream file("/proc/meminfo");
    while (file >> token) {
      if (token == "MemFree:") {
        unsigned long mem;
        if (file >> mem) {
          total_mem_kb = mem;
        } else {
          UMAP_ERROR("UMAP unable to determine system memory size\n");
        }
      }
      // ignore rest of the line
      file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
  }
  return ( ((total_mem_kb / (get_umap_page_size() / oneK)) * percent) / 100 );
}

void
RegionManager::set_max_pages_in_buffer( uint64_t max_pages )
{
  uint64_t max_pages_in_mem = get_max_pages_in_memory();
  uint64_t old_max_pages_in_buffer = get_max_pages_in_buffer();

  if ( max_pages > max_pages_in_mem ) {
    UMAP_ERROR("Cannot set maximum pages to "
        << max_pages
        << " because it must be less than the maximum pages in memory "
        << max_pages_in_mem);
  }

  m_max_pages_in_buffer = max_pages;

  UMAP_LOG(Debug,
    "Maximum pages in page buffer changed from "
    << old_max_pages_in_buffer
    << " to " << get_max_pages_in_buffer() << " pages");
}

void
RegionManager::set_read_ahead(uint64_t num_pages)
{
  m_read_ahead = num_pages;
}

void
RegionManager::set_umap_page_size( uint64_t page_size )
{
  //
  // Must be multiple of system page size
  //
  if ( page_size % get_system_page_size() ) {
    UMAP_ERROR("Specified page size (" << page_size
        << ") must be a multiple of the system page size ("
        << get_system_page_size() << ")");
  }

  UMAP_LOG(Debug,
      "Adjusting page size from "
      << get_umap_page_size() << " to " << page_size);

  m_umap_page_size = page_size;
}

uint64_t*
RegionManager::read_env_var( const char* env, uint64_t*  val )
{
  // return a pointer to val on success, null on failure
  char* val_ptr = 0;
  if ( (val_ptr = getenv(env)) ) {
    uint64_t env_val;

    std::string s(val_ptr);
    std::stringstream ss(s);
    ss >> env_val;

    if (env_val != 0) {
      *val = env_val;
      return val;
    }
  }
  return nullptr;
}

RegionDescriptor*
RegionManager::containing_region( char* vaddr )
{
  std::lock_guard<std::mutex> lock(m_mutex);

  //
  // Since the list of pages coming in are usually sorted, we have a special
  // check here to see if the region found for the previous check will work.
  // If this is the case, we can return early.
  //
  if ( m_last_iter != m_active_regions.end() ) {
    char* b = m_last_iter->second->start();
    char* e = m_last_iter->second->end();

    if ( vaddr >= b && vaddr < e )
      return m_last_iter->second;
  }

  auto iter = m_active_regions.upper_bound(reinterpret_cast<void*>(vaddr));

  if ( iter != m_active_regions.begin() ) {
    // Back up the iterator
    --iter;

    char* b = iter->second->start();
    char* e = iter->second->end();

    if ( vaddr >= b && vaddr < e ) {
      m_last_iter = iter;
      return iter->second;
    }
  }

  UMAP_LOG(Debug, "Unable to find addr: "
      << (void*)vaddr
      << " in region map. Ignoring"
  );

  return nullptr;
}

void
RegionManager::set_num_fillers( uint64_t num_fillers )
{
  m_num_fillers = num_fillers;
}

void
RegionManager::set_num_evictors( uint64_t num_evictors )
{
  m_num_evictors = num_evictors;
}
void
RegionManager::set_evict_high_water_threshold( int percent )
{
  m_evict_high_water_threshold = percent;
}
void
RegionManager::set_evict_low_water_threshold( int percent )
{
  m_evict_low_water_threshold = percent;
}
void
RegionManager::set_max_fault_events( uint64_t max_events )
{
  m_max_fault_events = max_events;
}
} // end of namespace Umap
