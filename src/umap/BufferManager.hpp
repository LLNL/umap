//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_BufferManager_HPP
#define _UMAP_BufferManager_HPP

#include <vector>
#include "umap/Buffer.hpp"
#include "umap/RegionDescriptor.hpp"
#include "umap/EvictWorkers.hpp"

namespace Umap {
class EvictWorkers;
class BufferManager {
  public:
    Buffer* getBufferManager( void* page );
    void evictAll( EvictWorkers* worker );
    void evict_region( RegionDescriptor* rd );

    explicit BufferManager( void );
    ~BufferManager( void );

  private:
    uint64_t m_size;          // Maximum pages this buffer may have
    PageDescriptor* m_array;
    std::vector<Buffer*> m_buffers;
};
} // end of namespace Umap

#endif // _UMAP_BufferManager_HPP
