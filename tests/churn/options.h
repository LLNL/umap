/* This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
#ifndef _OPTIONS_H
#define _OPTIONS_H
#include <stdint.h>
#include "options.h"

typedef struct {
  int iodirect;
  int usemmap;
  int initonly;
  int noinit;

  uint64_t page_buffer_size;    // # of pages that page buffer can hold

  uint64_t num_churn_pages;
  uint64_t num_load_pages;

  uint64_t num_churn_threads;

  uint64_t num_load_reader_threads;
  uint64_t num_load_writer_threads;

  char const* fn;               // Backing file name

  uint64_t testduration;        // Duration (in seconds) to run test
} options_t;

void getoptions(options_t&, int&, char **argv);
#endif // _OPTIONS_H
