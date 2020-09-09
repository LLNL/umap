//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_H_
#define _UMAP_H_

#ifdef __cplusplus
  #include <cstdint>
  #include <mutex>
  #include "umap/store/Store.hpp"
  #include "umap/store/StoreNetwork.h"
#else // __cplusplus
  #include <stdint.h>
#endif // __cplusplus

#include <unistd.h>
#include <sys/mman.h>

#ifdef __cplusplus
namespace Umap {
/** Allow application to create region of memory to a persistent store
 * \param addr Same as input argument for mmap(2)
 * \param length Same as input argument of mmap(2)
 * \param prot Same as input argument of mmap(2)
 * \param flags Same as input argument of mmap(2)
 */
extern std::mutex m_mutex;
extern int num_thread;
void* umap_ex(
    void*         addr
  , std::size_t   length
  , int           prot
  , int           flags
  , int           fd
  , off_t         offset
  , Umap::Store*  store
);
} // namespace Umap
#endif // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif
/** Allow application to create region of memory to a persistent store
 * \param addr Same as input argument for mmap(2)
 * \param length Same as input argument of mmap(2)
 * \param prot Same as input argument of mmap(2)
 * \param flags Same as input argument of mmap(2)
 */
void* umap(
    void* addr
  , size_t length
  , int prot
  , int flags
  , int fd
  , off_t offset
);

void* umap_network(
		   const char*  id
                   , void* region_addr
                   , size_t region_size
);

int uunmap(
    void*  addr
  , size_t length
);

int umap_flush(); 
int umap_evict();
  
struct umap_prefetch_item {
  void* page_base_addr;
};

void umap_prefetch( int npages, struct umap_prefetch_item* page_array );
uint64_t umapcfg_get_umap_page_size( void );
uint64_t umapcfg_get_max_fault_events( void );
uint64_t umapcfg_get_num_fillers( void );
uint64_t umapcfg_get_num_evictors( void );
uint64_t umapcfg_get_max_pages_in_buffer( void );
uint64_t umapcfg_get_read_ahead( void );
int      umapcfg_get_evict_low_water_threshold( void );
int      umapcfg_get_evict_high_water_threshold( void );

#ifdef __cplusplus
}
#endif

/*
 * flags
 */
#define UMAP_PRIVATE    MAP_PRIVATE // Note - UMAP_SHARED not currently supported
#define UMAP_FIXED      MAP_FIXED   // See mmap(2) - This flag is currently then only flag supported.

/*
 * Return codes
 */
#define UMAP_FAILED (void *)-1
#endif // _UMAP_H
