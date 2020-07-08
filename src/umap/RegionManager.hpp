//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_RegionManager_HPP
#define _UMAP_RegionManager_HPP

#include <cstdint>
#include <mutex>
#include <map>

#include "umap/Buffer.hpp"
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
    static RegionManager& getInstance( void );

    // delete copy, move, and assign operators
    RegionManager(RegionManager const&) = delete;             // Copy construct
    RegionManager(RegionManager&&) = delete;                  // Move construct
    RegionManager& operator=(RegionManager const&) = delete;  // Copy assign
    RegionManager& operator=(RegionManager &&) = delete;      // Move assign

    void addRegion(
          int fd
        , Store*   store
        , void*   region
        , uint64_t region_size
        , char*    mmap_region
        , uint64_t mmap_region_size
        , bool     server=false
        , int      clientfd=0
        , void*    remote_addr=NULL
    );
    char* associateRegion(
          int fd
        , void*   region
        , bool    server=false
        , int     clientfd=0
        , void*   remote_base=NULL 
    );

    void *isFDRegionPresent(int fd);
    int flush_buffer();
    void prefetch(int npages, umap_prefetch_item* page_array, int client_fd=0);
    bool removeRegion( char* mmap_region, int client_fd=0, int file_fd=0, bool client_term=false);
    void terminateUffdHandler(int client_fd);
    Uffd* getActiveUffd(bool server, int client_fd);
    Version  get_umap_version( void ) { return m_version; }
    long     get_system_page_size( void ) { return m_system_page_size; }
    uint64_t get_max_pages_in_buffer( void ) { return m_max_pages_in_buffer; }
    uint64_t get_read_ahead( void ) { return m_read_ahead; }
    uint64_t get_umap_page_size( void ) { return m_umap_page_size; }
    uint64_t get_num_fillers( void ) { return m_num_fillers; }
    uint64_t get_num_evictors( void ) { return m_num_evictors; }
    int get_evict_low_water_threshold( void ) { return m_evict_low_water_threshold; }
    int get_evict_high_water_threshold( void ) { return m_evict_high_water_threshold; }
    uint64_t get_max_fault_events( void ) { return m_max_fault_events; }
    Buffer* get_buffer_h() { return m_buffer; }
    Uffd* get_uffd_h() { return m_client_uffds[0]; }
    FillWorkers* get_fill_workers_h() { return m_fill_workers; }
    EvictManager* get_evict_manager() { return m_evict_manager; }
    RegionDescriptor* containing_region( char* vaddr );
    uint64_t get_num_active_regions( void ) { return (uint64_t)m_active_regions.size(); }
  private:
    Version  m_version;
    uint64_t m_max_pages_in_buffer;
    uint64_t m_read_ahead;
    long     m_umap_page_size;
    uint64_t m_system_page_size;
    uint64_t m_num_fillers;
    uint64_t m_num_evictors;
    int m_evict_low_water_threshold;
    int m_evict_high_water_threshold;
    uint64_t m_max_fault_events;
    Buffer* m_buffer;
    FillWorkers* m_fill_workers;
    EvictManager* m_evict_manager;
    std::mutex m_mutex;
    
    std::map<int, RegionDescriptor* > m_fd_rd_map;
    std::map<void*, RegionDescriptor*> m_active_regions;
    std::map<void*, RegionDescriptor*>::iterator m_last_iter;
    std::map<int, Uffd* > m_client_uffds;
    RegionManager( void );

    uint64_t* read_env_var( const char* env, uint64_t* val);
    uint64_t        get_max_pages_in_memory( void );
    void set_max_fault_events( uint64_t max_events );
    void set_max_pages_in_buffer( uint64_t max_pages );
    void set_read_ahead(uint64_t num_pages);
    void set_umap_page_size( uint64_t page_size );
    void set_num_fillers( uint64_t num_fillers );
    void set_num_evictors( uint64_t num_evictors );
    void set_evict_low_water_threshold( int percent );
    void set_evict_high_water_threshold( int percent );
    void setFDRegionMap(int file_fd, RegionDescriptor *rd);
};

} // end of namespace Umap
#endif // _UMAP_RegionManager_HPP
