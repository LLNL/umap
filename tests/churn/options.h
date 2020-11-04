//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
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
