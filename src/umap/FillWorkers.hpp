//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_FillWorkers_HPP
#define _UMAP_FillWorkers_HPP

#include "umap/config.h"

#include <errno.h>
#include <string.h>             // strerror()
#include <unistd.h>             // sleep()

#include "umap/Buffer.hpp"
#include "umap/Uffd.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"
#include "umap/util/PthreadPool.hpp"
#include "umap/util/WorkQueue.hpp"

namespace Umap {
struct FillWorkItem {
  Uffd* uffd;
  Store* store;
  void* region;
  std::size_t size;
  std::size_t alignsize;
  int fd;
};

class FillWorkers : PthreadPool {
  public:
    FillWorkers(uint64_t num_workers, Buffer* buffer, Uffd* uffd,
        Store* store, WorkQueue<FillWorkItem>* wq) :
          PthreadPool("Fill Workers", num_workers), m_buffer(buffer),
          m_uffd(uffd), m_store(store), m_wq(wq)
    {
      start_thread_pool();
    }

    ~FillWorkers( void )
    {
    }

  private:
    Buffer* m_buffer;
    Uffd* m_uffd;
    Store* m_store;
    WorkQueue<FillWorkItem>* m_wq;

    inline void ThreadEntry() {
      while ( ! time_to_stop_thread_pool() ) {
        sleep(1);
      }
    }
};
} // end of namespace Umap
#endif // _UMAP_FillWorkers_HPP
