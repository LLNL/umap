/*
This file was taken from Spindle and made part of Umpire.  The Spindle and
umpire boilerplate is included below.

This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at
https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or 
modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) 
version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY 
WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the 
GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 
59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
and conditions of the GNU Lesser General Public License for more details.  You should 
have received a copy of the GNU Lesser General Public License along with this 
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

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
