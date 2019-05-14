//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include <cstdint>
#include <errno.h>
#include <fcntl.h>
#include <iomanip>
#include <string.h>             // strerror()
#include <unistd.h>             // syscall()
#include <vector>
#include <linux/userfaultfd.h>  // ioctl(UFFDIO_*)
#include <sys/ioctl.h>          // ioctl()
#include <sys/syscall.h>        // syscall()

#include "umap/config.h"

#include "umap/Buffer.hpp"
#include "umap/FillManager.hpp"
#include "umap/FillWorkers.hpp"
#include "umap/EvictManager.hpp"
#include "umap/PageDescriptor.hpp"
#include "umap/RegionManager.hpp"
#include "umap/Uffd.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {
  FillManager::FillManager( void )
    :   m_page_size(RegionManager::getInstance()->get_umap_page_size())
      , m_read_ahead(RegionManager::getInstance()->get_read_ahead())
  {
  }

  ~FillManager::FillManager( void )
  {
    delete m_fill_workers;
    delete m_evict_manager;
    delete m_buffer;
  }

  void FillManager::new_page_event(const PageDescriptor& pd) {
    m_buffer->process_page_event(pd, m_fill_workers, m_evict_manager);

    if (m_read_ahead && pd.page_is_dirty() && m_buffer->evict_low_threshold_reached()) {
      char* end = (char*)(pd->region->get_end_of_region());
      char* paddr = (char*)(pd.get_page_addr());

      for ( int i = 0; i < m_read_ahead; ++i ) {
        PageDescriptor page_d;

        paddr += m_page_size;
        if (paddr >= end)
          break;

        page_d.m_page = (void*)paddr;
        page_d.m_store = pd.m_store;
        page_d.m_is_dirty = false;
        page_d.m_state = FREE;

        m_buffer->process_page_event(page_d, m_fill_workers, m_evict_manager);
      }
    }
  }
} // end of namespace Umap
