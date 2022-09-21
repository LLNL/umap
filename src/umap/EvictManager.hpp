//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_EvictManager_HPP
#define _UMAP_EvictManager_HPP

#include "umap/EvictWorkers.hpp"

#include "umap/Buffer.hpp"
#include "umap/PageDescriptor.hpp"
#include "umap/RegionDescriptor.hpp"
#include "umap/WorkerPool.hpp"

namespace Umap {
  class EvictWorkers;

  class EvictManager : public WorkerPool {
    public:
      EvictManager( RegionManager& rm );
      ~EvictManager( void );
      void schedule_eviction(PageDescriptor* pd);
      void schedule_flush(PageDescriptor* pd);
      void adapt_evict_workers(int max_workers);
      void EvictAll( void );
      void WaitAll( void );
      EvictWorkers* get_fill_workers() {return m_evict_workers;};

    private:
      Buffer* m_buffer;
      EvictWorkers* m_evict_workers;
      RegionManager& m_rm;

      void EvictMgr(void);
      void ThreadEntry( void );
  };
} // end of namespace Umap
#endif // _UMAP_EvictManager_HPP
