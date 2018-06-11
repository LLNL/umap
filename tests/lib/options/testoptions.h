/*
 * This file is part of UMAP.  For copyright information see the COPYRIGHT
 * file in the top level directory, or at
 * https://github.com/LLNL/umap/blob/master/COPYRIGHT
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (as published by the Free
 * Software Foundation) version 2.1 dated February 1999.  This program is
 * distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the terms and conditions of the GNU Lesser General Public License
 * for more details.  You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _UMAPTEST_H
#define _UMAPTEST_H
#include <stdint.h>

typedef struct {
  int initonly;
  int noinit;
  int iodirect;
  int usemmap;
  int noio;

  uint64_t numpages;
  uint64_t numthreads;
  uint64_t bufsize;
  char const* filename; // file name or basename
  char const* dirname; // dir name or basename
} umt_optstruct_t;

#ifdef __cplusplus
extern "C" {
#endif
  void umt_getoptions(umt_optstruct_t*, int, char *argv[]);
  long umt_getpagesize(void);
#ifdef __cplusplus
}
#endif
#endif // _UMAPTEST_H
