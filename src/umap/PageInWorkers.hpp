//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_PageInWorkers_HPP
#define _UMAP_PageInWorkers_HPP

#include "umap/config.h"

#include <pthread.h>
#include <vector>

#include "umap/Buffer.hpp"
#include "umap/Uffd.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"
#include "umap/util/WorkQueue.hpp"

namespace Umap {
  class PageInWorkers {
    public:
      PageInWorkers(
            Buffer* buffer
          , Uffd* uffd
          , Store* store
          , WorkQueue<PageInWorkItem>* wq
      );

      ~PageInWorkers( void );

    private:
      Buffer* m_buffer;
      Uffd* m_uffd;
      Store* m_store;
      WorkQueue<PageInWorkItem>* m_wq;
      bool m_time_to_stop;

      std::vector<pthread_t> m_page_in_threads;
      void start_threads();
      void stop_threads();
      void page_in_thread();
      static void* page_in_thread_starter(void * This);
  };
} // end of namespace Umap

#endif // _UMAP_PageInWorkers_HPP
