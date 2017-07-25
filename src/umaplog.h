/* This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
#ifndef _UMAPLOG_H_
#define _UMAPLOG_H_
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#ifdef DEBUG
// Usage: This logging facility is available in Debug builds of the library. It is enabled by setting the UMAP_LOGGING to a value (if unset, it will be disabled.
#include <stdio.h>

extern void __umaplog_init(void);
extern void umaplog_lock(void);
extern void umaplog_unlock(void);
extern bool umap_logging;

#define umapdbg(format, ...)\
    do {\
        if (umap_logging) {\
            umaplog_lock();\
            fprintf(stdout, format, ## __VA_ARGS__);\
            umaplog_unlock();\
        }\
    } while (0)

#define umaperr(format, ...)\
    do {\
        umaplog_lock();\
        fprintf(stderr, format, ## __VA_ARGS__);\
        umaplog_unlock();\
    } while (0)
#define umaplog_init __umaplog_init
#else
#define umaperr(format, ...)
#define umapdbg(format, ...)
#define umaplog_init()
#endif
#endif
