//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory
//
// Created by Marty McFadden, 'mcfadden8 at llnl dot gov'
// LLNL-CODE-733797
//
// All rights reserved.
//
// This file is part of UMAP.
//
// For details, see https://github.com/LLNL/umap
// Please also see the COPYRIGHT and LICENSE files for LGPL license.
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_H_
#define _UMAP_H_

#ifdef __cplusplus
  #include <cstdint>
  #include "umap/Store.h"
  #include <unistd.h>
  #include <sys/mman.h>
#else // __cplusplus
  #include <stdint.h>
  #include <unistd.h>
  #include <sys/mman.h>
#endif // __cplusplus


#ifdef __cplusplus
/** Allow application to create region of memory to a peristant store
 * \param addr Same as input argument for mmap(2)
 * \param length Same as input argument of mmap(2)
 * \param prot Same as input argument of mmap(2)
 * \param flags Same as input argument of mmap(2)
 * \param r_pstore pointer to callback function to be used for providing data from
 *                 persistent storage.
 * \param w_pstore pointer to callback function to be used for saving data to
 *                 persistent storage.
 */
void* umap_ex(
    void* addr,
    std::size_t length,
    int prot,
    int flags,
    int fd,
    off_t offset,
    Store*
);
#endif // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif
/** Allow application to create region of memory to a peristant store
 * \param addr Same as input argument for mmap(2)
 * \param length Same as input argument of mmap(2)
 * \param prot Same as input argument of mmap(2)
 * \param flags Same as input argument of mmap(2)
 * \param r_pstore pointer to callback function to be used for providing data from
 *                 persistent storage.
 * \param w_pstore pointer to callback function to be used for saving data to
 *                 persistent storage.
 */
void* umap(
    void* addr,
    size_t length,
    int prot,               /* See mmap(2) - Subset supported, rest ignored */
    int flags,              /* See mmap(2) - Subset supported, rest ignored */
    int fd,                 /* See mmap(2) */
    off_t offset            /* See mmap(2) - umap ignores this */
);

int uunmap( void*  addr,         /* See mmap(2) */
            size_t length        /* See mmap(2) */
        );

uint64_t* umap_cfg_readenv(const char* env, uint64_t* val);
void umap_cfg_getenv( void );
uint64_t umap_cfg_get_bufsize( void );
void umap_cfg_set_bufsize( uint64_t page_bufsize );
uint64_t umap_cfg_get_uffdthreads( void );
void umap_cfg_set_uffdthreads( uint64_t numthreads );
void umap_cfg_flush_buffer( void* region );
int umap_cfg_get_pagesize( void );
int umap_cfg_set_pagesize( long psize );

struct umap_cfg_stats {
    uint64_t dirty_evicts;
    uint64_t clean_evicts;
    uint64_t evict_victims;
    uint64_t wp_messages;
    uint64_t read_faults;
    uint64_t write_faults;
    uint64_t sigbus;
    uint64_t stuck_wp;
    uint64_t dropped_dups;
};

void umap_cfg_get_stats(void* region, struct umap_cfg_stats* stats);
void umap_cfg_reset_stats(void* region);

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
