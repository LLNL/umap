///////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_EvictWorkers_HPP
#define _UMAP_EvictWorkers_HPP

#include "umap/config.h"

#include "umap/PageDescriptor.hpp"
#include "umap/Uffd.hpp"
#include "umap/WorkerPool.hpp"

namespace Umap {
  class Uffd;
  class EvictWorkers : public WorkerPool {
    public:
      EvictWorkers(uint64_t num_evictors, Uffd* uffd);
      ~EvictWorkers( void );

    private:
      Uffd* m_uffd;

      void EvictWorker( void );
      void ThreadEntry( void );
  };
} // end of namespace Umap
#endif // _UMAP_EvictWorkers_HPP
