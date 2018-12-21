//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory
//
// Created by Marty McFadden, 'mcfadden8 at llnl dot gov'
// LLNL-CODE-733797
//
// All rights reserved.
//
// This file is part of UMAP.
//
// For details, see https://github.com/LLNL/umap
// Please also see the COPYRIGHT and LICENSE files for LGPL license.
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
