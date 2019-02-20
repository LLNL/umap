//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

#if !defined(UMAP_SPINDLE_DEBUG_H_)
#define UMAP_SPINDLE_DEBUG_H_

#include <stdio.h>
#include <unistd.h>
#include "config.h"

#if defined(UMAP_DEBUG_LOGGING)

#if defined(__cplusplus)
extern "C" {
#endif
#include "spindle_logc.h"
#if defined(__cplusplus)
}
#endif

#define LOGGING_INIT init_spindle_debugging(0)
#define LOGGING_INIT_PREEXEC init_spindle_debugging(1)
#define LOGGING_FINI fini_spindle_debugging()

#else
#define LOGGING_INIT
#define LOGGING_INIT_PREEXEC
#define LOGGING_FINI
#define debug_printf(format, ...)
#define debug_printf2(S, ...) debug_printf(S, ## __VA_ARGS__)
#define debug_printf3(S, ...) debug_printf(S, ## __VA_ARGS__)

#define bare_printf(S, ...) debug_printf(S, ## __VA_ARGS__)
#define bare_printf2(S, ...) debug_printf(S, ## __VA_ARGS__)
#define bare_printf3(S, ...) debug_printf(S, ## __VA_ARGS__)

#define err_printf(S, ...) debug_printf(S, ## __VA_ARGS__)
#endif

#endif
