//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include "umap/config.h"

#include <errno.h>
#include <pthread.h>
#include <string.h>             // strerror()
#include <vector>

#include "umap/Buffer.hpp"
#include "umap/Uffd.hpp"
#include "umap/PageInWorkers.hpp"
#include "umap/util/Macros.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/WorkQueue.hpp"

namespace Umap {

  PageInWorkers::PageInWorkers(
        Buffer* buffer
      , Uffd* uffd
      , Store* store
      , WorkQueue<PageInWorkItem>* wq
    ): m_buffer(buffer), m_uffd(uffd), m_store(store), m_wq(wq), m_time_to_stop(false)
  {
    // Launch threads here...
  }

  PageInWorkers::~PageInWorkers( void )
  {
    // Wait for threads to go away here
  }

#include <unistd.h>

  void PageInWorkers::page_in_thread() {
    UMAP_LOG(Debug, "\nThe Worker says hello: ");

    while ( ! m_time_to_stop ) {
      sleep(1);
    }

    UMAP_LOG(Debug, "Goodbye");
  }

  void PageInWorkers::start_threads()
  {
    pthread_t pt;

    int error = pthread_create( &pt, NULL, page_in_thread_starter, this);

    if (error)
      UMAP_ERROR("pthread_create failed: " << strerror(error));

    m_page_in_threads.push_back(pt);
  }

  void PageInWorkers::stop_threads()
  {
    m_time_to_stop = true;
    for ( auto pt : m_page_in_threads ) {
      UMAP_LOG(Debug, "Stoping " << (void*)pt);
      (void) pthread_join(pt, NULL);
    }

  }

  void* PageInWorkers::page_in_thread_starter(void * This) {
    ((PageInWorkers *)This)->page_in_thread();
    return NULL;
  }
} // end of namespace Umap
