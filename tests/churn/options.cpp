//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

#include <iostream>     // cout/cerr
#include <unistd.h>     // getopt()
#include <getopt.h>     // duh...
#include "options.h"
#include "umap/umap.h"

static char const* FILENAME = "/tmp/abc";
static const uint64_t NUMCHURNPAGES = 99;
static const uint64_t NUMLOADPAGES = 1;
static const uint64_t NUMCHURNTHREADS = 48;
static const uint64_t NUMLOADREADERS = 48;
static const uint64_t NUMLOADWRITERS = 48;
static const uint64_t TESTDURATION = 60;

using namespace std;

static void usage(char* pname)
{
  cerr
  << "Usage: " << pname << " [Options...]\n\n"
  << " --help                       - This message\n"
  << " --initonly                   - Initialize only\n"
  << " --noinit                     - No Initialization\n"
  << " --directio                   - Use O_DIRECT for file IO\n"
  << " --usemmap                    - Use mmap instead of umap\n"
  << " -b # of pages in page buffer - default: " << umapcfg_get_max_pages_in_buffer() << " Pages\n"
  << " -c # of churn pages          - default: " << NUMCHURNPAGES << " Pages\n"
  << " -l # of load pages           - default: " << NUMLOADPAGES << " Pages\n"
  << " -t # of churn threads        - default: " << NUMCHURNTHREADS << endl
  << " -r # of load reader threads  - default: " << NUMLOADREADERS << endl
  << " -w # of load writer threads  - default: " << NUMLOADWRITERS << endl
  << " -f [backing file name]       - default: " << FILENAME << endl
  << " -d # seconds to run test     - default: " << TESTDURATION << " seconds\n"
  << " \n"
  << " Environment Variable Configuration:\n"
  << " UMAP_PAGE_FILLERS(env) - currently: " << umapcfg_get_num_fillers() << " fillers\n"
  << " UMAP_PAGE_EVICTORS(env)- currently: " << umapcfg_get_num_evictors() << " evictors\n"
  << " UMAP_BUFSIZE(env)      - currently: " << umapcfg_get_max_pages_in_buffer() << " pages\n"
  << " UMAP_PAGESIZE(env)     - currently: " << umapcfg_get_umap_page_size() << " bytes\n"
  ;
  exit(1);
}

void getoptions(options_t& testops, int& argc, char **argv)
{
  int c;
  char *pname = argv[0];

  testops.iodirect=0;
  testops.usemmap=0;
  testops.noinit=0;
  testops.initonly=0;
  testops.num_churn_pages=NUMCHURNPAGES;
  testops.num_churn_threads=NUMCHURNTHREADS;
  testops.num_load_pages=NUMLOADPAGES;
  testops.num_load_reader_threads=NUMLOADREADERS;
  testops.num_load_writer_threads=NUMLOADWRITERS;
  testops.fn=FILENAME;
  testops.testduration=TESTDURATION;
  testops.page_buffer_size = umapcfg_get_max_pages_in_buffer();

  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
      {"directio",  no_argument,  &testops.iodirect, 1 },
      {"usemmap",   no_argument,  &testops.usemmap,  1 },
      {"initonly",  no_argument,  &testops.initonly, 1 },
      {"noinit",    no_argument,  &testops.noinit,   1 },
      {"help",      no_argument,  NULL,  0 },
      {0,           0,            0,     0 }
    };

    c = getopt_long(argc, argv, "b:c:l:t:r:w:f:d:", long_options, &option_index);
    if (c == -1)
      break;

    switch(c) {
      case 0:
        if (long_options[option_index].flag != 0)
          break;

        usage(pname);
        break;

      case 'b':
        if ((testops.page_buffer_size = strtoull(optarg, nullptr, 0)) > 0)
          break;
        goto R0;
      case 'c':
        if ((testops.num_churn_pages = strtoull(optarg, nullptr, 0)) > 0)
          break;
        goto R0;
      case 'l':
        if ((testops.num_load_pages = strtoull(optarg, nullptr, 0)) > 0)
          break;
        goto R0;

      case 'd':
        if ((testops.testduration = strtoull(optarg, nullptr, 0)) > 0)
          break;
        goto R0;
      case 'f':
        testops.fn = optarg;
        break;
      case 'w':
        testops.num_load_writer_threads = strtoull(optarg, nullptr, 0);
        break;
      case 'r':
        testops.num_load_reader_threads = strtoull(optarg, nullptr, 0);
        break;
        goto R0;
      case 't':
        testops.num_churn_threads = strtoull(optarg, nullptr, 0);
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
}

