//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_PageOutWorkers_HPP
#define _UMAP_PageOutWorkers_HPP

#include "umap/config.h"

#include <vector>

#include "umap/Buffer.hpp"
#include "umap/util/Macros.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/PthreadPool.hpp"
#include "umap/util/WorkQueue.hpp"

namespace Umap {
  class PageOutWorkers : PthreadPool {
    public:
      PageOutWorkers(
            uint64_t num_workers
          , Buffer* buffer
          , Store* store
          , WorkQueue<PageOutWorkItem>* wq
      );

      virtual ~PageOutWorkers( void );

    private:
      Buffer* m_buffer;
      Store* m_store;
      WorkQueue<PageOutWorkItem>* m_wq;

      void ThreadEntry();
  };
} // end of namespace Umap

#endif // _UMAP_PageOutWorkers_HPP
