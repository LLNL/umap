//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include "umap/config.h"

#include <cstdint>        // uint64_t
#include <fstream>        // for reading meminfo
#include <stdlib.h>       // getenv()
#include <sstream>        // string to integer operations
#include <string>         // string to integer operations
#include <thread>         // for max_concurrency
#include <unordered_map>
#include <unistd.h>       // sysconf()

#include "umap/FillManager.hpp"
#include "umap/Region.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {

Region* Region::s_fault_monitor_manager_instance = nullptr;
static const uint64_t MAX_FAULT_EVENTS = 256;

Region* Region::getInstance( void )
{
  if (!s_fault_monitor_manager_instance)
    s_fault_monitor_manager_instance = new Region();

  return s_fault_monitor_manager_instance;
}

void Region::makeFillManager(
    Store*   store
  , char*    region
  , uint64_t region_size
  , char*    mmap_region
  , uint64_t mmap_region_size
)
{
  m_active_umaps[(void*)region] = new FillManager(
                                                  store
                                                , region
                                                , region_size
                                                , mmap_region
                                                , mmap_region_size
                                                , get_umap_page_size()
                                                , get_max_fault_events()
                                            );
}

void
Region::destroyFillManager( char* region )
{
  UMAP_LOG(Debug, "region: " << (void*)region);

  auto it = m_active_umaps.find(region);

  if (it == m_active_umaps.end())
    UMAP_ERROR("umap fault monitor not found for: " << (void*)region);

  delete it->second;
  m_active_umaps.erase(it);
}

Region::Region()
{
  m_version.major = UMAP_VERSION_MAJOR;
  m_version.minor = UMAP_VERSION_MINOR;
  m_version.patch = UMAP_VERSION_PATCH;

  m_system_page_size = sysconf(_SC_PAGESIZE);

  uint64_t env_value = 0;
  if ( (read_env_var("UMAP_MAX_FAULT_EVENTS", &env_value)) != nullptr )
    set_max_fault_events(env_value);
  else
    set_max_fault_events(Umap::MAX_FAULT_EVENTS);

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
Region::get_max_pages_in_memory( void )
{
  static uint64_t total_mem_kb = 0;
  const uint64_t oneK = 1024;
  const uint64_t percent = 95;  // 95% of memory is max

  // Lazily set total_mem_kb global
  if ( ! total_mem_kb ) {
    std::string token;
    std::ifstream file("/proc/meminfo");
    while (file >> token) {
      if (token == "MemTotal:") {
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
Region::set_max_pages_in_buffer( uint64_t max_pages )
{
  if ( m_active_umaps.size() != 0 ) {
    UMAP_ERROR("Cannot change configuration when umaps are active");
  }

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
Region::set_read_ahead(uint64_t num_pages)
{
  if ( m_active_umaps.size() != 0 )
    UMAP_ERROR("Cannot change configuration when umaps are active");

  m_read_ahead = num_pages;

  UMAP_LOG(Debug,
    "Read ahead set to: " 
    << " to " << m_read_ahead << " pages");
}

void
Region::set_umap_page_size( uint64_t page_size )
{
  if ( m_active_umaps.size() != 0 ) {
    UMAP_ERROR("Cannot change configuration when umaps are active");
  }

  /*
   * Must be multiple of system page size
   */
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

uint64_t* Region::read_env_var(
    const char* env
  , uint64_t*  val
)
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

void
Region::set_num_fillers( uint64_t num_fillers )
{
  if ( m_active_umaps.size() != 0 ) {
    UMAP_ERROR("Cannot change configuration when umaps are active");
  }

  m_num_fillers = num_fillers;
}

void
Region::set_num_evictors( uint64_t num_evictors )
{
  if ( m_active_umaps.size() != 0 ) {
    UMAP_ERROR("Cannot change configuration when umaps are active");
  }

  m_num_evictors = num_evictors;
}

void
Region::set_evict_high_water_threshold( int percent )
{
  if ( m_active_umaps.size() != 0 ) {
    UMAP_ERROR("Cannot change configuration when umaps are active");
  }

  m_evict_high_water_threshold = percent;
}

void
Region::set_evict_low_water_threshold( int percent )
{
  if ( m_active_umaps.size() != 0 ) {
    UMAP_ERROR("Cannot change configuration when umaps are active");
  }

  m_evict_low_water_threshold = percent;
}

void
Region::set_max_fault_events( uint64_t max_events )
{
  if ( m_active_umaps.size() != 0 ) {
    UMAP_ERROR("Cannot change configuration when umaps are active");
  }

  m_max_fault_events = max_events;
}

} // end of namespace Umap
