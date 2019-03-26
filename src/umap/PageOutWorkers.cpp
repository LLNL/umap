//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

#include "umap/config.h"

#include "umap/Buffer.hpp"
#include "umap/Uffd.hpp"
#include "umap/PageOutWorkers.hpp"
#include "umap/util/Macros.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/PthreadPool.hpp"
#include "umap/util/WorkQueue.hpp"

namespace Umap {
  PageOutWorkers::PageOutWorkers(
        uint64_t num_workers
      , Buffer* buffer
      , Uffd* uffd
      , Store* store
      , WorkQueue<PageOutWorkItem>* wq
    ):   PthreadPool("Page Out Workers", num_workers)
       , m_buffer(buffer)
       , m_store(store)
       , m_wq(wq)
  {
    start_thread_pool();
  }

  PageOutWorkers::~PageOutWorkers( void )
  {
  }

  void PageOutWorkers::ThreadEntry() {
    while ( ! time_to_stop_thread_pool() ) {
      sleep(1);
    }
  }
} // end of namespace Umap
