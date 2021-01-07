//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_FillWorkers_HPP
#define _UMAP_FillWorkers_HPP

#include "umap/Buffer.hpp"
#include "umap/Uffd.hpp"
#include "umap/WorkerPool.hpp"

namespace Umap {
  class Buffer;
  class Uffd;

  class FillWorkers : public WorkerPool {
    public:
      FillWorkers( void );
      ~FillWorkers( void );

    private:
      Uffd*    m_uffd;
      Buffer*  m_buffer;

      void FillWorker( void );
      void ThreadEntry( void );
  };
} // end of namespace Umap
#endif // _UMAP_FillWorker_HPP
