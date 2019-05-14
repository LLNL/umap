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

#include "umap/Buffer.hpp"
#include "umap/FillWorkers.hpp"
#include "umap/EvictManager.hpp"
#include "umap/PageDescriptor.hpp"
#include "umap/Uffd.hpp"
#include "umap/WorkerPool.hpp"

namespace Umap {
  class FillManager {
    public:
      FillManager( void );
      ~FillManager( void );
      void new_page_event(const PageDescriptor& pd);

    private:
      uint64_t  m_page_size;
      int       m_read_ahead;
      FillWorkers* m_fill_workers;
      EvictManager* m_evict_manager;
      Buffer* m_buffer;
  };
}       // end of namespace Umap
#endif  // _UMAP_FillManager_HPP
