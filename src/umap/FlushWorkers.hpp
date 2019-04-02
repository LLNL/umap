//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_FlushWorkers_HPP
#define _UMAP_FlushWorkers_HPP

#include "umap/config.h"

#include "umap/Buffer.hpp"
#include "umap/Uffd.hpp"
#include "umap/util/Macros.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/PthreadPool.hpp"
#include "umap/util/WorkQueue.hpp"

namespace Umap {

struct FlushWorkItem {
  Store* store;
  void* region;
  std::size_t size;
  std::size_t alignsize;
  int fd;
};

class FlushWorkers : PthreadPool {
  public:
    FlushWorkers(
          uint64_t num_workers, Buffer* buffer, Uffd* uffd, Store* store, WorkQueue<FlushWorkItem>* wq) :
            PthreadPool("Flush Workers", num_workers), m_buffer(buffer), m_store(store), m_wq(wq)
    {
      start_thread_pool();
    }

    ~FlushWorkers( void )
    {
    }

  private:
    Buffer* m_buffer;
    Uffd* m_uffd;
    Store* m_store;
    WorkQueue<FlushWorkItem>* m_wq;

    inline void ThreadEntry() {
      while ( ! time_to_stop_thread_pool() ) {
        sleep(1);
      }
    }
};
} // end of namespace Umap

#endif // _UMAP_FlushWorkers_HPP
