//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_RegionManager_HPP
#define _UMAP_RegionManager_HPP

#include <cstdint>
#include <unordered_map>

#include "umap/BufferManager.hpp"
#include "umap/EvictManager.hpp"
#include "umap/FillWorkers.hpp"
#include "umap/Uffd.hpp"
#include "umap/umap.h"
#include "umap/store/Store.hpp"
#include "umap/RegionDescriptor.hpp"

namespace Umap {
class FillWorkers;
class EvictManager;

struct Version {
  int major;
  int minor;
  int patch;
};

//
// Implemented as a singleton for now.  Things can get too weird attempting to
// manage changes in configuration parameters when we have active monitors
// working.  So, we only allow changing configuration when there are no active
// monitors
//
class RegionManager {
  public:
    static RegionManager* getInstance( void );

    void addRegion(
          Store*   store
        , char*    region
        , uint64_t region_size
        , char*    mmap_region
        , uint64_t mmap_region_size
    );

    void prefetch(int npages, umap_prefetch_item* page_array);
    void removeRegion( char* mmap_region );
    Version  get_umap_version( void ) { return m_version; }
    long     get_system_page_size( void ) { return m_system_page_size; }
    uint64_t get_pages_per_buffer( void ) { return m_pages_per_buffer; }
    uint64_t get_number_of_buffers( void ) { return m_number_of_buffers; }
    uint64_t get_read_ahead( void ) { return m_read_ahead; }
    uint64_t get_umap_page_size( void ) { return m_umap_page_size; }
    uint64_t get_num_fillers( void ) { return m_num_fillers; }
    uint64_t get_num_evictors( void ) { return m_num_evictors; }
    int get_evict_low_water_threshold( void ) { return m_evict_low_water_threshold; }
    int get_evict_high_water_threshold( void ) { return m_evict_high_water_threshold; }
    uint64_t get_max_fault_events( void ) { return m_max_fault_events; }
    BufferManager* get_buffer_manager_h() { return m_buffer_manager; }
    Uffd* get_uffd_h() { return m_uffd; }
    FillWorkers* get_fill_workers_h() { return m_fill_workers; }
    EvictManager* get_evict_manager() { return m_evict_manager; }
    RegionDescriptor* containing_region( char* vaddr );
    uint64_t get_num_active_regions( void ) { return (uint64_t)m_active_regions.size(); }

  private:
    Version  m_version;
    uint64_t m_pages_per_buffer;
    uint64_t m_read_ahead;
    long     m_umap_page_size;
    uint64_t m_system_page_size;
    uint64_t m_number_of_buffers;
    uint64_t m_num_fillers;
    uint64_t m_num_evictors;
    int m_evict_low_water_threshold;
    int m_evict_high_water_threshold;
    uint64_t m_max_fault_events;
    BufferManager* m_buffer_manager;
    Uffd* m_uffd;
    FillWorkers* m_fill_workers;
    EvictManager* m_evict_manager;

    std::unordered_map<void*, RegionDescriptor*> m_active_regions;

    static RegionManager* s_fault_monitor_manager_instance;

    RegionManager( void );

    uint64_t* read_env_var( const char* env, uint64_t* val);
    uint64_t        get_max_pages_in_memory( void );
    void set_max_fault_events( uint64_t max_events );
    void set_pages_per_buffer( uint64_t pages );
    void set_number_of_buffers( uint64_t num_buffers );
    void set_read_ahead(uint64_t num_pages);
    void set_umap_page_size( uint64_t page_size );
    void set_num_fillers( uint64_t num_fillers );
    void set_num_evictors( uint64_t num_evictors );
    void set_evict_low_water_threshold( int percent );
    void set_evict_high_water_threshold( int percent );
};

} // end of namespace Umap
#endif // _UMAP_RegionManager_HPP
