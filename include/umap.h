/*
 * This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at
 * https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) 
 * version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
 * and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the 
 * GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _UMAP_H_
#define _UMAP_H_
#include <sys/types.h>
#include <sys/mman.h>

#ifdef __cplusplus
extern "C" {
#endif
/** Signatures for application provided callbacks to read/write data from/to 
 * persistant storage.
 *
 * \param region Returned from previous umap() call
 * \param dblbuf Buffer provided and owned by umap to read data in to.  This
 * buffer must page-aligned and at least 2X the size of the # of bytes being
 * requested.  This is for buffering purposes in the event that the source
 * file is not page-aligned.
 * \param nbytes # of bytes to read/write in/from \a dblbuf. The # of bytes
 * must be a multiple of the system page size.
 * \param region_offset Byte offset from beginning of \a region
 * \returns If successful, the number of bytes read/written in/from \a buf.
 * Otherwise, -1.
 *
 * \b Note: This callback is assumed to be threadsafe.  Since this is
 * a "C" interface, this will need to be inforced by convention and
 * implementations that are thread-unsafe may not function correctly.
 */
typedef ssize_t (*umap_pstore_read_f_t)(
    void* region,
    char* dblbuf,
    size_t nbytes,
    off_t region_offset
    );

typedef ssize_t (*umap_pstore_write_f_t)(
    void* region,
    char* buf,
    size_t nbytes,
    off_t region_offset
    );

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
    int prot,
    int flags,
    umap_pstore_read_f_t r_pstore,
    umap_pstore_write_f_t w_pstore
);

int uunmap( void*  addr,    /* See mmap(2) */
            size_t length   /* See mmap(2) */
        );

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
