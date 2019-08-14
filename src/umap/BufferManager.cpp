//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

#include <vector>

#include "umap/Buffer.hpp"
#include "umap/BufferManager.hpp"
#include "umap/config.h"
#include "umap/EvictWorkers.hpp"
#include "umap/PageDescriptor.hpp"
#include "umap/RegionDescriptor.hpp"
#include "umap/RegionManager.hpp"
#include "umap/util/hash.hpp"
#include "umap/util/Macros.hpp"

namespace Umap {

Buffer* BufferManager::getBufferManager( void* page )
{
  uint64_t key  = reinterpret_cast<uint64_t>(page);
  uint64_t data = util::hash<uint64_t>()(key);

  data = data % m_buffers.size();

  return m_buffers[data];
}

void BufferManager::evictAll( EvictWorkers* workers )
{
  for ( auto buf : m_buffers ) {
    for (auto pd = buf->evict_oldest_page(); pd != nullptr; pd = buf->evict_oldest_page()) {
      UMAP_LOG(Debug, "evicting: " << pd);
      if (pd->dirty) {
        WorkItem work = { .page_desc = pd, .buffer = buf, .type = Umap::WorkItem::WorkType::FAST_EVICT };
        workers->send_work(work);
      }
      else {
        buf->mark_page_as_free(pd);
      }
    }
  }

  workers->wait_for_idle();
}

void BufferManager::evict_region( RegionDescriptor* rd )
{
  for ( auto it : m_buffers )
    it->evict_region(rd);
}

BufferManager::BufferManager( void )
{
  auto rm = RegionManager::getInstance();
  auto num_buffers = rm->get_number_of_buffers();
  m_size = num_buffers * rm->get_pages_per_buffer();

  m_array = (PageDescriptor *)calloc(m_size, sizeof(PageDescriptor));
  if ( m_array == nullptr )
    UMAP_ERROR("Failed to allocate " << m_size*sizeof(PageDescriptor)
        << " bytes for buffer page descriptors");

  UMAP_LOG(Debug, "Creating " << num_buffers << " buffers.");
  for ( int i = 0; i < num_buffers; ++i) {
    m_buffers.push_back(new Buffer(&m_array[i*rm->get_pages_per_buffer()]));
  }
}

BufferManager::~BufferManager( void ) {
  auto rm = RegionManager::getInstance();
  auto num_buffers = rm->get_number_of_buffers();
  BufferStats stat;

  for ( int i = 0; i < num_buffers; ++i) {
    stat = stat + m_buffers[i]->getStats();
    delete m_buffers[i];
  }
  free(m_array);

#ifdef UMAP_DISPLAY_STATS
  std::cout << stat << std::endl;
#endif
}

} // end of namespace Umap
