//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_FillManager_HPP
#define _UMAP_FillManager_HPP

#include "umap/config.h"

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
#include "umap/FillWorkers.hpp"
#include "umap/EvictManager.hpp"
#include "umap/Region.hpp"
#include "umap/Uffd.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {
  class FillManager : public WorkerPool {
    private:
      void FillMgr() {
        UMAP_LOG(Debug,    "\n             m_store: " <<  (void*)m_store
                        << "\n         m_page_size: " <<  m_page_size
                        << "\n  m_max_fault_events: " <<  m_max_fault_events
        );

        uint64_t read_ahead = Region::getInstance()->get_read_ahead();

        while ( 1 ) {
          auto pe = m_uffd->get_page_events();

          if (pe.size() == 0)
            continue;

          if ( pe[0].aligned_page_address == (char*)nullptr && pe[0].is_write_fault == false) {
            UMAP_LOG(Debug, "Good-bye");
            break;
          }

          int count = 0;
          for ( auto & event : pe ) {
            m_buffer->process_page_event(event.aligned_page_address
                , event.is_write_fault
                , m_fill_workers
                , m_evict_manager
                , m_store
            );

            //
            // Do read ahead if possible
            //
            if (!event.is_write_fault && read_ahead && m_buffer->evict_low_threshold_reached()) {
              char* paddr = (char*)(event.aligned_page_address);
              char* end = (char*)(m_uffd->end_of_region_for_page((void*)paddr));

              for ( int i = 0; i < read_ahead; ++i ) {
                paddr += m_page_size;
                if (paddr >= end)
                  break;

                m_buffer->process_page_event((void*)paddr
                  , false
                  , m_fill_workers
                  , m_evict_manager
                  , m_store
                );
              }
            }
          }
        }
      }

      Store*    m_store;
      uint64_t  m_page_size;
      uint64_t  m_max_fault_events;

      Uffd* m_uffd;

      FillWorkers* m_fill_workers;

      EvictManager* m_evict_manager;
      Buffer* m_buffer;

      void ThreadEntry() {
        FillMgr();
      }

    public:
      FillManager(
                Store*   store
              , char*    region
              , uint64_t region_size
              , char*    mmap_region
              , uint64_t mmap_region_size
              , uint64_t page_size
              , uint64_t max_fault_events
            ) :   WorkerPool("Fill Manager", 1)
                , m_store(store)
                , m_page_size(page_size)
                , m_max_fault_events(max_fault_events)
      {
        m_uffd = new Uffd(region, region_size, max_fault_events, page_size);

        m_buffer = new Buffer(
              Region::getInstance()->get_max_pages_in_buffer()
            , Region::getInstance()->get_evict_low_water_threshold()
            , Region::getInstance()->get_evict_high_water_threshold()
        );

        m_fill_workers = new FillWorkers(m_uffd, m_buffer);

        m_evict_manager = new EvictManager(
              Region::getInstance()->get_num_evictors()
            , m_buffer, m_uffd, m_store
        );

        start_thread_pool();
      }

      ~FillManager( void )
      {
        m_uffd->stop_uffd();
        stop_thread_pool();
        delete m_fill_workers;
        delete m_evict_manager;
        delete m_buffer;
        delete m_uffd;
      }
  };
} // end of namespace Umap

#endif // _UMAP_FillManager_HPP
