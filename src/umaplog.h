/* This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
#ifndef _UMAPLOG_H_
#define _UMAPLOG_H_
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE
//
// Usage: This logging facility is available in Debug builds of the library. It is enabled by setting the UMAP_LOGGING to a value (if unset, it will be disabled.
#include <stdio.h>
#include <time.h>

extern void __umaplog_init(void);
extern void umaplog_lock(void);
extern void umaplog_unlock(void);
extern bool umap_logging;

#define umaperr(format, ...)\
    do {\
        struct timespec t;\
        char _s[120];\
        (void)clock_gettime(CLOCK_MONOTONIC_RAW, &t);\
        umaplog_lock();\
        sprintf(_s, "%ld.%09ld " format, t.tv_sec, t.tv_nsec, ## __VA_ARGS__);\
        fprintf(stderr, "%s", _s);\
        umaplog_unlock();\
    } while (0)
#define umaplog_init __umaplog_init

#ifdef DEBUG
#define umapdbg(format, ...)\
    do {\
        if (umap_logging) {\
            struct timespec t;\
            char _s[120];\
            (void)clock_gettime(CLOCK_MONOTONIC_RAW, &t);\
            umaplog_lock();\
            sprintf(_s, "%ld.%09ld " format, t.tv_sec, t.tv_nsec, ## __VA_ARGS__);\
            fprintf(stdout, "%s", _s);\
            umaplog_unlock();\
        }\
    } while (0)
#else
#define umapdbg(format, ...)
#endif
#endif
