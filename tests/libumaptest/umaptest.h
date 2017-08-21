/*
This file is part of UMAP.  For copyright information see the COPYRIGHT
file in the top level directory, or at
https://github.com/LLNL/umap/blob/master/COPYRIGHT
This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free
Software Foundation) version 2.1 dated February 1999.  This program is
distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the terms and conditions of the GNU Lesser General Public License
for more details.  You should have received a copy of the GNU Lesser General
Public License along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
#ifndef _UMAPTEST_H
#define _UMAPTEST_H
#include <stdint.h>

typedef struct {
  int initonly;
  int noinit;
  int iodirect;
  int usemmap;

  uint64_t numpages;
  uint64_t numthreads;
  uint64_t bufsize;
  char const* fn;
  int fnum;
} umt_optstruct_t;

#ifdef __cplusplus
extern "C" {
#endif
  void umt_getoptions(umt_optstruct_t*, int, char *argv[]);
  int umt_openandmap(const umt_optstruct_t*, uint64_t, void**);
  void umt_closeandunmap(const umt_optstruct_t*, uint64_t, void*, int);
  long umt_getpagesize(void);
  void* umt_openandmap_fits(const umt_optstruct_t*, uint64_t, void**,off_t,off_t);
  void umt_closeandunmap_fits(const umt_optstruct_t*, uint64_t, void*,void*);
  void* umt_openandmap_fits2(const umt_optstruct_t*, uint64_t, void**,off_t,off_t);
  void umt_closeandunmap_fits2(const umt_optstruct_t*, uint64_t, void**,void*);
#ifdef __cplusplus
}
#endif
#endif // _UMAPTEST_H
