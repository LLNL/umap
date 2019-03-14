//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

#include "umap/config.h"

#include "umap/Buffer.hpp"
#include "umap/PageOutWorkers.hpp"
#include "umap/util/Macros.hpp"
#include "umap/store/Store.hpp"
#include "umap/util/WorkQueue.hpp"

namespace Umap {
  PageOutWorkers::PageOutWorkers(
        Buffer* buffer
      , Store* store
      , WorkQueue<PageOutWorkItem>* wq
    ): m_buffer(buffer), m_store(store), m_wq(wq)
  {
    // Launch threads here...
  }

  PageOutWorkers::~PageOutWorkers( void )
  {
    // Wait for threads to go away here
  }

} // end of namespace Umap
