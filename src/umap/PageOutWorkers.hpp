//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_PageOutWorkers_HPP
#define _UMAP_PageOutWorkers_HPP

#include "umap/config.h"

#include <pthread.h>
#include <vector>

#include "umap/Buffer.hpp"
#include "umap/util/Macros.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/WorkQueue.hpp"

namespace Umap {
  class PageOutWorkers {
    public:
      PageOutWorkers(
            Buffer* buffer
          , Store* store
          , WorkQueue<PageOutWorkItem>* wq
      );

      ~PageOutWorkers( void );

    private:
      Buffer* m_buffer;
      Store* m_store;
      WorkQueue<PageOutWorkItem>* m_wq;

      std::vector<pthread_t> m_page_out_threads;
      void start_thread();
      void stop_thread();
      void page_out_thread();
      static void* page_out_thread_starter(void * This);
  };
} // end of namespace Umap

#endif // _UMAP_PageOutWorkers_HPP
