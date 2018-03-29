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

typedef struct umap_backing_file {
    int fd;
    off_t data_size;
    off_t data_offset;    /* Offset of data portion in file */
} umap_backing_file;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * umap() is a wrapper around mmap(2) and userfaultfd(2) to allow for creating a mapping of pages managed in user-space.
 */
void* umap( void*  addr,    /* See mmap(2) */
            size_t length,  /* See mmap(2) */
            int    prot,    /* See mmap(2) */
            int    flags,   /* See below, see mmap(2) for general notes */
            int    fd,      /* See mmap(2) */
            off_t  offset   /* See mmap(2) */
        );
int uunmap( void*  addr,    /* See mmap(2) */
            size_t length   /* See mmap(2) */
        );

void* umap_mf(void* addr, 
      size_t        length, 
      int           prot, 
      int           flags,
      int           num_backing_files,
      umap_backing_file* backing_files
        );

uint64_t umap_cfg_get_bufsize( void );
void umap_cfg_set_bufsize( uint64_t page_bufsize );
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
#endif // _UMAP_H_
