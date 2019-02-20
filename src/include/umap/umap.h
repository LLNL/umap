//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
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
    uint64_t evict_victims;
    uint64_t wp_messages;
    uint64_t read_faults;
    uint64_t write_faults;
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
