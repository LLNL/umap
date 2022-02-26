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
#ifdef LOCK_OPT
static int t_id_g=0;
#endif

namespace Umap {
  void FillWorkers::FillWorker( void ){
    char* copyin_buf;
    uint64_t page_size = RegionManager::getInstance().get_umap_page_size();
    std::size_t sz = page_size;
    int t_id = t_id_g;
    t_id_g ++;
    printf("FillWorker t_id %d Enter FillWorker\n", t_id);


    if (posix_memalign((void**)&copyin_buf, page_size, sz)) {
      UMAP_ERROR("posix_memalign failed to allocated "
          << sz << " bytes of memory");
    }

    if (copyin_buf == nullptr) {
      UMAP_ERROR("posix_memalign failed to allocated "
          << sz << " bytes of memory");
    }

    while ( 1 ) {

      auto w = get_work(t_id);

      #ifdef PROF
      auto t0 = std::chrono::steady_clock::now();
      UMAP_LOG(Info, "from send till get_work \t" << (std::chrono::duration_cast<std::chrono::nanoseconds>(t0-w.timing).count()) );
      #endif
      
      //printf("FillWorker t_id %d paddr %p\n", t_id, w.page_desc->page);

      //UMAP_LOG(Info, " t_id "<<t_id<<" : " << w << " " << m_buffer);

      if (w.type == Umap::WorkItem::WorkType::EXIT)
        break;    // Time to leave

      if ( w.page_desc->dirty && w.page_desc->data_present ) {
        m_uffd->disable_write_protect(w.page_desc->page);
      }
      else {
        uint64_t offset = w.page_desc->region->store_offset(w.page_desc->page);

        #ifdef PROF1
        auto t00 = std::chrono::steady_clock::now();
        #endif

        if (w.page_desc->region->store()->read_from_store(copyin_buf, page_size, offset) == -1)
          UMAP_ERROR("read_from_store failed");

        #ifdef PROF1
        auto t01 = std::chrono::steady_clock::now();
        UMAP_LOG(Info, "read_from_store \t" << (std::chrono::duration_cast<std::chrono::nanoseconds>(t01-t00).count()) );
        #endif        

        if ( ! w.page_desc->dirty ) {
          m_uffd->copy_in_page_and_write_protect(copyin_buf, w.page_desc->page);
        }
        else {
          m_uffd->copy_in_page(copyin_buf, w.page_desc->page);
        }
        w.page_desc->data_present = true;
        
        #ifdef PROF1
        auto t02 = std::chrono::steady_clock::now();
        UMAP_LOG(Info, "copy_in_page \t" << (std::chrono::duration_cast<std::chrono::nanoseconds>(t02-t01).count()) );
        #endif 
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
      , m_uffd(RegionManager::getInstance().get_uffd_h())
      , m_buffer(RegionManager::getInstance().get_buffer_h())
  {
    start_thread_pool();
  }

  FillWorkers::~FillWorkers( void ) {
    stop_thread_pool();
  }
} // end of namespace Umap
