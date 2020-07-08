//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include "umap/config.h"

#include <cstdint>              // calloc
#include <errno.h>
#include <string.h>             // strerror()
#include <unistd.h>

#include "umap/Buffer.hpp"
#include "umap/FillWorkers.hpp"
#include "umap/RegionManager.hpp"
#include "umap/Uffd.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {
  void FillWorkers::FillWorker( void ) {
    char* copyin_buf;
    Uffd* c_uffd;
    uint64_t page_size = RegionManager::getInstance().get_umap_page_size();
    uint64_t read_ahead = RegionManager::getInstance().get_read_ahead();
    std::size_t sz = 2 * page_size;

    if (posix_memalign((void**)&copyin_buf, page_size, sz)) {
      UMAP_ERROR("posix_memalign failed to allocated "
          << sz << " bytes of memory");
    }

    if (copyin_buf == nullptr) {
      UMAP_ERROR("posix_memalign failed to allocated "
          << sz << " bytes of memory");
    }

    while ( 1 ) {
      auto w = get_work();
      c_uffd = (Uffd*)w.c_uffd;

      UMAP_LOG(Debug, ": " << w << " " << m_buffer);

      if (w.type == Umap::WorkItem::WorkType::EXIT)
        break;    // Time to leave


      if( w.page_desc->data_present){
        if ( w.page_desc->dirty ) {
          c_uffd->disable_write_protect(w.page_desc->page);
        }
        else{ 
          c_uffd->wake_up_range(w.page_desc->page);
          continue;
        }
      }
      else{
        uint64_t offset = w.page_desc->region->store_offset(w.page_desc->page);

        if (w.page_desc->region->store()->read_from_store(copyin_buf, page_size, offset) == -1)
          UMAP_ERROR("read_from_store failed");

        if ( ! w.page_desc->dirty ) {
          c_uffd->copy_in_page_and_write_protect(copyin_buf, w.page_desc->page);
        }
        else {
          c_uffd->copy_in_page(copyin_buf, w.page_desc->page);
        }
        w.page_desc->data_present = true;
      }

      m_buffer->mark_page_as_present(w.page_desc);
    }

    free(copyin_buf);
  }

  void FillWorkers::ThreadEntry( void ) {
    FillWorker();
  }

  FillWorkers::FillWorkers( void )
    :   WorkerPool("Fill Workers", RegionManager::getInstance().get_num_fillers())
      , m_buffer(RegionManager::getInstance().get_buffer_h())
      , m_read_ahead(RegionManager::getInstance().get_read_ahead())
  {
    start_thread_pool();
  }

  FillWorkers::~FillWorkers( void ) {
    stop_thread_pool();
  }
} // end of namespace Umap
