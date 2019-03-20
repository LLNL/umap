//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_PageInWorkers_HPP
#define _UMAP_PageInWorkers_HPP

#include "umap/config.h"

#include <vector>

#include "umap/Buffer.hpp"
#include "umap/Uffd.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"
#include "umap/util/PthreadPool.hpp"
#include "umap/util/WorkQueue.hpp"

namespace Umap {
  class PageInWorkers : PthreadPool {
    public:
      PageInWorkers(
            uint64_t num_workers
          , Buffer* buffer
          , Uffd* uffd
          , Store* store
          , WorkQueue<PageInWorkItem>* wq
      );

      virtual ~PageInWorkers();

    private:
      Buffer* m_buffer;
      Uffd* m_uffd;
      Store* m_store;
      WorkQueue<PageInWorkItem>* m_wq;

      void ThreadEntry();
  };
} // end of namespace Umap

#endif // _UMAP_PageInWorkers_HPP
