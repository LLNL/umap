//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
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
  int shuffle;

  long pagesize;
  uint64_t numpages;
  uint64_t numthreads;
  uint64_t bufsize;
  uint64_t uffdthreads;
  uint64_t pages_to_access;  // 0 (default) - access all pages
  char const* filename; // file name or basename
  char const* dirname; // dir name or basename
} umt_optstruct_t;

static char const* DIRNAME = "/mnt/intel/";
static char const* FILENAME = "abc";
const uint64_t NUMPAGES = 10000000;
const uint64_t NUMTHREADS = 2;
const uint64_t BUFFERSIZE = 16;

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
  << " --shuffle              - Shuffle memory accesses (instead of sequential access)\n"
  << " -p # of pages          - default: " << NUMPAGES << endl
  << " -t # of threads        - default: " << NUMTHREADS << endl
  << " -u # of page fillers   - default: " << umapcfg_get_num_fillers() << " UFFD page fillers\n"
  << " -b # page buffer size  - default: " << umapcfg_get_max_pages_in_buffer() << " Pages\n"
  << " -a # pages to access   - default: 0 - access all pages\n"
  << " -f [file name]         - backing file name.  Or file basename if multiple files\n"
  << " -d [directory name]    - backing directory name.  Or dir basename if multiple dirs\n"
  << " -P # page size         - default: " << umapcfg_get_umap_page_size() << endl;
  exit(1);
}

void umt_getoptions(utility::umt_optstruct_t* testops, int argc, char *argv[])
{
  int c;
  char *pname = argv[0];

  testops->initonly = 0;
  testops->noinit = 0;
  testops->usemmap = 0;
  testops->shuffle = 0;
  testops->pages_to_access = 0;
  testops->numpages = NUMPAGES;
  testops->numthreads = NUMTHREADS;
  testops->bufsize = umapcfg_get_max_pages_in_buffer();
  testops->uffdthreads = umapcfg_get_num_fillers();
  testops->filename = FILENAME;
  testops->dirname = DIRNAME;
  testops->pagesize = umapcfg_get_umap_page_size();

  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
      {"initonly",  no_argument,  &testops->initonly, 1 },
      {"noinit",    no_argument,  &testops->noinit,   1 },
      {"usemmap",   no_argument,  &testops->usemmap,  1 },
      {"shuffle",   no_argument,  &testops->shuffle,  1 },
      {"help",      no_argument,  NULL,  0 },
      {0,           0,            0,     0 }
    };

    c = getopt_long(argc, argv, "p:t:f:b:d:u:a:P:", long_options, &option_index);
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
          umapcfg_set_umap_page_size(testops->pagesize);
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
      case 'a':
        testops->pages_to_access = strtoull(optarg, nullptr, 0);
        break;
      case 'd':
        testops->dirname = optarg;
        break;
      case 'f':
        testops->filename = optarg;
        break;
      default:
      R0:
        usage(pname);
    }
  }

  if (testops->numpages < testops->pages_to_access) {
    cerr << "Invalid -a argument " << testops->pages_to_access << "\n";
    usage(pname);
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
  if (testops->uffdthreads != umapcfg_get_num_fillers()) {
    umapcfg_set_num_fillers(testops->uffdthreads);
    umapcfg_set_num_evictors(testops->uffdthreads);
  }

  umapcfg_set_max_pages_in_buffer(testops->bufsize);
}

long umt_getpagesize(void)
{
  return umapcfg_get_umap_page_size();
}
}
#endif // _COMMANDLING_HPP
