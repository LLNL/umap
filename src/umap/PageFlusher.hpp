//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_PageFlusher_HPP
#define _UMAP_PageFlusher_HPP

#include "umap/config.h"

#include <vector>

#include "umap/Buffer.hpp"
#include "umap/Flushers.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/Uffd.hpp"
#include "umap/WorkQueue.hpp"
#include "umap/util/Macros.hpp"
#include "umap/store/Store.hpp"

namespace Umap {
class PageFlusher : WorkerPool {
public:
PageFlusher(
      uint64_t num_flushers, Buffer* buffer, Uffd* uffd, Store* store) :
        WorkerPool("Page Flusher", 1), m_buffer(buffer), m_uffd(uffd), m_store(store)
{
  m_flush_wq = new WorkQueue<FlushWorkItem>;
  m_page_flushers = new Flushers(PageRegion::getInstance()->get_num_flushers(), m_buffer, m_uffd , m_store, m_flush_wq);

  start_thread_pool();
}

~PageFlusher( void )
{
  delete m_flush_wq;
  delete m_page_flushers;
}

private:
Buffer* m_buffer;
Uffd* m_uffd;
Store* m_store;
WorkQueue<FlushWorkItem>* m_flush_wq;
Flushers* m_page_flushers;

inline void ThreadEntry() {
  while ( ! time_to_stop_thread_pool() ) {
    std::vector<PageDescriptor*> page_descs;

    sleep(1);
  }
}
};
} // end of namespace Umap

#endif // _UMAP_PageFlusher_HPP
