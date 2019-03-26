//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include "umap/config.h"

#include <errno.h>
#include <string.h>             // strerror()
#include <unistd.h>             // sleep()

#include "umap/Buffer.hpp"
#include "umap/Uffd.hpp"
#include "umap/PageInWorkers.hpp"
#include "umap/util/Macros.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/PthreadPool.hpp"
#include "umap/util/WorkQueue.hpp"

namespace Umap {

  PageInWorkers::PageInWorkers(
        uint64_t num_workers
      , Buffer* buffer
      , Uffd* uffd
      , Store* store
      , WorkQueue<PageInWorkItem>* wq
    ) :   PthreadPool("Page In Workers", num_workers)
        , m_buffer(buffer)
        , m_uffd(uffd)
        , m_store(store)
        , m_wq(wq)
  {
    start_thread_pool();
  }

  PageInWorkers::~PageInWorkers( void )
  {
  }

  void PageInWorkers::ThreadEntry() {
    while ( ! time_to_stop_thread_pool() ) {
      sleep(1);
    }
  }
} // end of namespace Umap
