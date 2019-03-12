//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_FaultMonitor_HPP
#define _UMAP_FaultMonitor_HPP

#include <cstdint>
#include <vector>

#include "umap/store/Store.hpp"
#include "umap/util/Pthread.hpp"

namespace Umap {

class Pthread;

class FaultMonitor {
  public:
    FaultMonitor(
          Store* store
        , char*        region
        , uint64_t     region_size
        , char*        mmap_region
        , uint64_t     mmap_region_size
        , uint64_t     page_size
        , uint64_t     max_fault_events
    );

    ~FaultMonitor( void );

  private:
    Store*   m_store;
    char*    m_region;
    uint64_t m_region_size;
    char*    m_mmap_region;
    uint64_t m_mmap_region_size;
    uint64_t m_page_size;
    uint64_t m_max_fault_events;
    int      m_uffd_fd;
    Pthread* m_monitor;

    void check_uffd_compatibility( void );
    void register_uffd( void );
};
} // end of namespace Umap

#endif // _UMAP_FaultMonitor_HPP
