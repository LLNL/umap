//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_EvictManager_HPP
#define _UMAP_EvictManager_HPP

#include "umap/Buffer.hpp"
#include "umap/BufferManager.hpp"
#include "umap/EvictWorkers.hpp"
#include "umap/PageDescriptor.hpp"
#include "umap/RegionDescriptor.hpp"
#include "umap/WorkerPool.hpp"

namespace Umap {
  class EvictWorkers;
  class BufferManager;
  class Buffer;

  class EvictManager : public WorkerPool {
    public:
      EvictManager( void );
      ~EvictManager( void );
      void schedule_eviction(PageDescriptor* pd, Buffer* bd);
      void EvictAll( void );

    private:
      BufferManager* m_buffer_manager;
      EvictWorkers* m_evict_workers;

      void EvictMgr(void);
      void ThreadEntry( void );
  };
} // end of namespace Umap
#endif // _UMAP_EvictManager_HPP
