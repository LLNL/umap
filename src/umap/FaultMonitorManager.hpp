//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_FaultMonitorManager_HPP
#define _UMAP_FaultMonitorManager_HPP

#include <cstdint>
#include <unordered_map>

#include "umap/FaultMonitor.hpp"
#include "umap/store/Store.hpp"

namespace Umap {

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
class FaultMonitorManager {
  public:
    static FaultMonitorManager* getInstance( void );

    void makeFaultMonitor(
          Store*   store
        , char*    region
        , uint64_t region_size
        , char*    mmap_region
        , uint64_t mmap_region_size
    );

    void destroyFaultMonitor( char* mmap_region );

    inline Version  get_umap_version( void )         { return m_version; }

    inline long     get_system_page_size( void )     { return m_system_page_size; }

    inline uint64_t get_max_pages_in_buffer( void )  { return m_max_pages_in_buffer; }
    void set_max_pages_in_buffer( uint64_t max_pages );

    inline uint64_t get_umap_page_size( void )       { return m_umap_page_size; }
    void set_umap_page_size( uint64_t page_size );

    inline uint64_t get_num_page_in_workers( void )  { return m_num_page_in_workers; }
    void set_num_page_in_workers( uint64_t num_workers );

    inline uint64_t get_num_page_out_workers( void ) { return m_num_page_out_workers; }
    void set_num_page_out_workers( uint64_t num_workers );

    inline uint64_t get_max_fault_events( void )     { return m_max_fault_events; }
    void set_max_fault_events( uint64_t max_events );

  private:
    Version  m_version;
    uint64_t m_max_pages_in_buffer;
    long     m_umap_page_size;
    uint64_t m_system_page_size;
    uint64_t m_num_page_in_workers;
    uint64_t m_num_page_out_workers;
    uint64_t m_max_fault_events;
    std::unordered_map<void*, FaultMonitor*> m_active_umaps;

    static FaultMonitorManager* s_fault_monitor_manager_instance;

    FaultMonitorManager( void );

    uint64_t* read_env_var( const char* env, uint64_t* val);
    uint64_t        get_max_pages_in_memory( void );
};
} // end of namespace Umap
#endif // UMPIRE_FaultMonitorManager_HPP
