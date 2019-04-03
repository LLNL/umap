//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_PageRegion_HPP
#define _UMAP_PageRegion_HPP

#include <cstdint>
#include <unordered_map>

#include "umap/store/Store.hpp"

namespace Umap {

class PageFiller;

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
class PageRegion {
  public:
    static PageRegion* getInstance( void );

    void makePageFiller(
          Store*   store
        , char*    region
        , uint64_t region_size
        , char*    mmap_region
        , uint64_t mmap_region_size
    );

    void destroyPageFiller( char* mmap_region );

    inline Version  get_umap_version( void )         { return m_version; }

    inline long     get_system_page_size( void )     { return m_system_page_size; }

    inline uint64_t get_max_pages_in_buffer( void )  { return m_max_pages_in_buffer; }
    void set_max_pages_in_buffer( uint64_t max_pages );

    inline uint64_t get_umap_page_size( void )       { return m_umap_page_size; }
    void set_umap_page_size( uint64_t page_size );

    inline uint64_t get_num_fillers( void )  { return m_num_fillers; }
    void set_num_fillers( uint64_t num_fillers );

    inline uint64_t get_num_flushers( void ) { return m_num_flushers; }
    void set_num_flushers( uint64_t num_flushers );

    inline uint64_t get_max_fault_events( void )     { return m_max_fault_events; }
    void set_max_fault_events( uint64_t max_events );

  private:
    Version  m_version;
    uint64_t m_max_pages_in_buffer;
    long     m_umap_page_size;
    uint64_t m_system_page_size;
    uint64_t m_num_fillers;
    uint64_t m_num_flushers;
    uint64_t m_max_fault_events;

    std::unordered_map<void*, PageFiller*> m_active_umaps;

    static PageRegion* s_fault_monitor_manager_instance;

    PageRegion( void );

    uint64_t* read_env_var( const char* env, uint64_t* val);
    uint64_t        get_max_pages_in_memory( void );
};
} // end of namespace Umap
#endif // UMPIRE_PageRegion_HPP
