/*
 * This file is part of UMAP.  For copyright information see the COPYRIGHT file
 * in the top level directory, or at
 * https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free
 * software; you can redistribute it and/or modify it under the terms of the
 * GNU Lesser General Public License (as published by the Free Software
 * Foundation) version 2.1 dated February 1999.  This program is distributed in
 * the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the terms and conditions of the GNU Lesser General Public License for
 * more details.  You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _UMAP_H_
#define _UMAP_H_
#include <cstdint>
#include <unistd.h>
#include <sys/mman.h>
#include "Store.h"

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
    std::size_t length,
    int prot,               /* See mmap(2) - Subset supported, rest ignored */
    int flags,              /* See mmap(2) - Subset supported, rest ignored */
    int fd,                 /* See mmap(2) */
    off_t offset            /* See mmap(2) - umap ignores this */
);

int uunmap( void*  addr,         /* See mmap(2) */
            std::size_t length   /* See mmap(2) */
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
