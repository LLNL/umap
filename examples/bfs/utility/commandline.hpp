/*
 * This file is part of UMAP.  For copyright information see the COPYRIGHT file
 * in the top level directory, or at
 * https://github.com/LLNL/umap/blob/master/COPYRIGHT. This program is free
 * software; you can redistribute it and/or modify it under the terms of the
 * GNU Lesser General Public License (as published by the Free Software
 * Foundation) version 2.1 dated February 1999.  This program is distributed in
 * the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the terms and conditions of the GNU Lesser General Public License for more
 * details.  You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
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
  uint64_t bufsize;
  uint64_t numpages;
  uint64_t numthreads;
  int64_t  numfiles;
  uint64_t num_filler_threads;
  uint64_t num_evictor_threads;
  uint64_t evict_hiwater;
  uint64_t evict_lowater;

  uint64_t pages_to_access;  // 0 (default) - access all pages
  char const* filename; // file name or basename
  char const* dirname; // dir name or basename
} umt_optstruct_t;

static char const* DIRNAME = "/mnt/intel/";
static char const* FILENAME = "abc";
const uint64_t NUMPAGES = 10000000;
const uint64_t NUMTHREADS = 2;
const uint64_t BUFFERSIZE = 16;
const int64_t NUMFILES = 1;

using namespace std;

static void usage(char* pname)
{
  std::cerr
  << "Usage: " << pname << " [--initonly] [--noinit] [--directio]"
  <<                       " [--usemmap] [-p #] [-t #] [-b #] [-f name]\n\n"
  << " --help                      - This message\n"
  << " --initonly                  - Initialize file, then stop\n"
  << " --noinit                    - Use previously initialized file\n"
  << " --usemmap                   - Use mmap instead of umap\n"
  << " --shuffle                   - Shuffle memory accesses (instead of sequential access)\n"
  << " -p # of pages               - default: " << NUMPAGES << std::endl
  << " -t # of app threads         - default: " << NUMTHREADS << std::endl
  << " -a # pages to access        - default: 0 - access all pages\n"
  << " -N # of files               - default: " << NUMFILES << std::endl
  << " -f [file name]              - backing file name.  Or file basename if multiple files\n"
  << " -d [directory name]         - backing directory name.  Or dir basename if multiple dirs\n"
  << std::endl
  << " Environment Variable Configuration (command line arguments obsolete):\n"
  << " UMAP_PAGESIZE                   - currently: " << umapcfg_get_umap_page_size() << " bytes\n"
  << " UMAP_PAGE_FILLERS               - currently: " << umapcfg_get_num_fillers() << " fillers\n"
  << " UMAP_PAGE_EVICTORS              - currently: " << umapcfg_get_num_evictors() << " evictors\n"
  << " UMAP_BUFSIZE                    - currently: " << umapcfg_get_max_pages_in_buffer() << " pages\n"
  << " UMAP_EVICT_LOW_WATER_THRESHOLD  - currently: " << umapcfg_get_evict_low_water_threshold() << " percent full\n"
  << " UMAP_EVICT_HIGH_WATER_THRESHOLD - currently: " << umapcfg_get_evict_high_water_threshold() << " percent full\n"
  << std::endl;
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
  testops->num_filler_threads = umapcfg_get_num_fillers();
  testops->num_evictor_threads = umapcfg_get_num_evictors();
  testops->filename = FILENAME;
  testops->dirname = DIRNAME;
  testops->numfiles = NUMFILES;
  testops->pagesize = umapcfg_get_umap_page_size();
  testops->evict_lowater = umapcfg_get_evict_low_water_threshold();
  testops->evict_hiwater = umapcfg_get_evict_high_water_threshold();

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

    c = getopt_long(argc, argv, "p:t:f:d:a:N:", long_options, &option_index);
    if (c == -1)
      break;

    switch(c) {
      case 0:
        if (long_options[option_index].flag != 0)
          break;

        usage(pname);
        break;
      case 'p':
        if ((testops->numpages = strtoull(optarg, nullptr, 0)) > 0)
          break;
        goto R0;
      case 'N':
        if ((testops->numfiles = strtoull(optarg, nullptr, 0)) > 0)
          break;
        goto R0;
      case 't':
        if ((testops->numthreads = strtoull(optarg, nullptr, 0)) > 0)
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
    std::cerr << "Invalid -a argument " << testops->pages_to_access << "\n";
    usage(pname);
  }

  if (optind < argc) {
    std::cerr << "Unknown Arguments: ";
    while (optind < argc)
      std::cerr << "\"" << argv[optind++] << "\" ";
    std::cerr << std::endl;
    usage(pname);
  }
}

long umt_getpagesize(void)
{
  return umapcfg_get_umap_page_size();
}
}
#endif // _COMMANDLING_HPP
