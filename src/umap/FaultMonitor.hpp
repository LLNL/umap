//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_FaultMonitor_HPP
#define _UMAP_FaultMonitor_HPP

#include "umap/config.h"

#include <cstdint>
#include <vector>

#include "umap/PageInWorkers.hpp"
#include "umap/PageOutWorkers.hpp"
#include "umap/Uffd.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/PthreadPool.hpp"
#include "umap/util/WorkQueue.hpp"

namespace Umap {
  class FaultMonitor : PthreadPool {
    public:
      FaultMonitor(
            Store*   store
          , char*    region
          , uint64_t region_size
          , char*    mmap_region
          , uint64_t mmap_region_size
          , uint64_t page_size
          , uint64_t max_fault_events
      );

      ~FaultMonitor( void );

    protected:
      void ThreadEntry();

    private:
      Store*    m_store;
      char*     m_region;
      uint64_t  m_region_size;
      char*     m_mmap_region;
      uint64_t  m_mmap_region_size;
      uint64_t  m_page_size;
      uint64_t  m_max_fault_events;
      int       m_uffd_fd;

      Uffd* m_uffd;

      WorkQueue<PageInWorkItem>*  m_pagein_wq;
      PageInWorkers* m_pagein_workers;

      WorkQueue<PageOutWorkItem>* m_pageout_wq;
      PageOutWorkers* m_pageout_workers;
  };
} // end of namespace Umap

#endif // _UMAP_FaultMonitor_HPP
