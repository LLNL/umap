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
#ifndef _COMMANDLING_HPP
#define _COMMANDLING_HPP

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <stdint.h>
#include <iostream>     // cout/cerr
#include <unistd.h>     // getopt()
#include <getopt.h>     // duh...
#include "umap/umap.h"

namespace utility {
typedef struct {
  int initonly;       // Just perform initialization, then quit
  int noinit;         // Init already done, so skip it
  int usemmap;

  long pagesize;
  uint64_t numpages;
  uint64_t numthreads;
  uint64_t bufsize;
  uint64_t uffdthreads;
  char const* filename; // file name or basename
} umt_optstruct_t;

static char const* FILENAME = "testfile";
const uint64_t NUMPAGES = 10000000;
const uint64_t NUMTHREADS = 2;

using namespace std;

static void usage(char* pname)
{
  cerr
  << "Usage: " << pname << " [--initonly] [--noinit] [--directio]"
  <<                       " [--usemmap] [-p #] [-t #] [-b #] [-f name]\n\n"
  << " --help                 - This message\n"
  << " --initonly             - Initialize file, then stop\n"
  << " --noinit               - Use previously initialized file\n"
  << " --usemmap              - Use mmap instead of umap\n"
  << " -p # of pages          - default: " << NUMPAGES << endl
  << " -t # of threads        - default: " << NUMTHREADS << endl
  << " -u # of uffd threads   - default: " << umap_cfg_get_uffdthreads() << " worker threads\n"
  << " -b # page buffer size  - default: " << umap_cfg_get_bufsize() << " Pages\n"
  << " -a # pages to access   - default: 0 - access all pages\n"
  << " -f [file name]         - backing file name.\n"
  << " -P # page size         - default: " << umap_cfg_get_pagesize() << endl;
  exit(1);
}

void umt_getoptions(utility::umt_optstruct_t* testops, int argc, char *argv[])
{
  int c;
  char *pname = argv[0];

  testops->initonly = 0;
  testops->noinit = 0;
  testops->usemmap = 0;
  testops->numpages = NUMPAGES;
  testops->numthreads = NUMTHREADS;
  testops->bufsize = umap_cfg_get_bufsize();
  testops->uffdthreads = umap_cfg_get_uffdthreads();
  testops->filename = FILENAME;
  testops->pagesize = umap_cfg_get_pagesize();

  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
      {"initonly",  no_argument,  &testops->initonly, 1 },
      {"noinit",    no_argument,  &testops->noinit,   1 },
      {"usemmap",   no_argument,  &testops->usemmap,  1 },
      {"help",      no_argument,  NULL,  0 },
      {0,           0,            0,     0 }
    };

    c = getopt_long(argc, argv, "p:t:f:b:u:P:", long_options, &option_index);
    if (c == -1)
      break;

    switch(c) {
      case 0:
        if (long_options[option_index].flag != 0)
          break;

        usage(pname);
        break;

      case 'P':
        if ((testops->pagesize = strtol(optarg, nullptr, 0)) > 0) {
          if (umap_cfg_set_pagesize(testops->pagesize) < 0) {
            goto R0;
          }
          break;
        }
        goto R0;
      case 'p':
        if ((testops->numpages = strtoull(optarg, nullptr, 0)) > 0)
          break;
        goto R0;
      case 't':
        if ((testops->numthreads = strtoull(optarg, nullptr, 0)) > 0)
          break;
        else goto R0;
      case 'b':
        if ((testops->bufsize = strtoull(optarg, nullptr, 0)) > 0)
          break;
        else goto R0;
      case 'u':
        if ((testops->uffdthreads = strtoull(optarg, nullptr, 0)) > 0)
          break;
        else goto R0;
      case 'f':
        testops->filename = optarg;
        break;
      default:
      R0:
        usage(pname);
    }
  }

  if (optind < argc) {
    cerr << "Unknown Arguments: ";
    while (optind < argc)
      cerr << "\"" << argv[optind++] << "\" ";
    cerr << endl;
    usage(pname);
  }

  /*
   * Note: Care must be taken when configuring the number of threads
   * and the buffer size of umap.  When the buffer size is set, it
   * apportions the buffer evenly to the umap threads.  So setting the
   * buffer size requires that the number of threads be set properly
   * first.
   */
  if (testops->uffdthreads != umap_cfg_get_uffdthreads())
    umap_cfg_set_uffdthreads(testops->uffdthreads);

  umap_cfg_set_bufsize(testops->bufsize);
}

long umt_getpagesize(void)
{
  return umap_cfg_get_pagesize();
}
}
#endif // _COMMANDLING_HPP
