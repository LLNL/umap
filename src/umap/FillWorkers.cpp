//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
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

static int t_id_g=0;

namespace Umap {
  void FillWorkers::FillWorker( void ){
    char* copyin_buf;
    std::size_t max_page_size = (std::size_t) m_rm.get_max_page_size();
    int t_id = t_id_g;
    t_id_g ++;

    if (posix_memalign((void**)&copyin_buf, 4096, max_page_size)) {
      UMAP_ERROR("posix_memalign failed to allocated "
          << max_page_size << " bytes of memory");
    }

    if (copyin_buf == nullptr) {
      UMAP_ERROR("posix_memalign failed to allocated "
          << max_page_size << " bytes of memory");
    }

    while ( 1 ) {

      auto w = get_work(t_id);
      if (w.type == Umap::WorkItem::WorkType::EXIT)
        break;    // Time to leave      

      #ifdef PROF
      auto t0 = std::chrono::steady_clock::now();
      UMAP_LOG(Info, "from send till get_work \t" << (std::chrono::duration_cast<std::chrono::nanoseconds>(t0-w.timing).count()) );
      UMAP_LOG(Info, "t_id "<<t_id<<" : " << w << " buffer: " << m_buffer);
      #endif

      RegionDescriptor *rd = w.page_desc->region;
      uint64_t psize = rd->page_size();
      char    *paddr = w.page_desc->page;

      if ( w.page_desc->dirty && w.page_desc->data_present ) {
        m_uffd->disable_write_protect(psize, paddr);
      }
      else {
        uint64_t offset = w.page_desc->region->store_offset(paddr);

        if (w.page_desc->region->store()->read_from_store(copyin_buf, psize, offset) == -1)
          UMAP_ERROR("read_from_store failed");      

        if ( ! w.page_desc->dirty ) {
          m_uffd->copy_in_page_and_write_protect(copyin_buf, paddr, psize);
        }
        else {
          m_uffd->copy_in_page(copyin_buf, paddr, psize);
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

  FillWorkers::FillWorkers( RegionManager& rm )
    :   WorkerPool("Fill Workers", rm.get_num_fillers())
      , m_uffd(rm.get_uffd_h())
      , m_buffer(rm.get_buffer_h())
      , m_rm ( rm )
  {
    start_thread_pool();
  }

  FillWorkers::~FillWorkers( void ) {
    stop_thread_pool();
  }
} // end of namespace Umap
