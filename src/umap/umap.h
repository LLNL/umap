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
  #include "umap/store/Store.hpp"
#else // __cplusplus
  #include <stdint.h>
#endif // __cplusplus

#include <unistd.h>
#include <sys/mman.h>

#ifdef __cplusplus
namespace Umap {
/** Allow application to create region of memory to a peristant store
 * \param addr Same as input argument for mmap(2)
 * \param length Same as input argument of mmap(2)
 * \param prot Same as input argument of mmap(2)
 * \param flags Same as input argument of mmap(2)
 */
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
/** Allow application to create region of memory to a peristant store
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

int uunmap(
    void*  addr
  , size_t length
);

long     umapcfg_get_system_page_size( void );

uint64_t umapcfg_get_max_pages_in_buffer( void );
void     umapcfg_set_max_pages_in_buffer( uint64_t max_pages );

uint64_t umapcfg_get_umap_page_size( void );
void     umapcfg_set_umap_page_size( uint64_t page_size );

uint64_t umapcfg_get_num_fillers( void );
void     umapcfg_set_num_fillers( uint64_t num_fillers );

uint64_t umapcfg_get_num_evictors( void );
void     umapcfg_set_num_evictors( uint64_t num_evictors );

int umapcfg_get_evict_low_water_threshold( void );
void     umapcfg_set_evict_low_water_threshold( int threshold_percentage );

int umapcfg_get_evict_high_water_threshold( void );
void     umapcfg_set_evict_high_water_threshold( int threshold_percentage );

uint64_t umapcfg_get_max_fault_events( void );
void     umapcfg_set_max_fault_events( uint64_t max_events );

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
