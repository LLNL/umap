/* This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

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
  << " -b # of pages in page buffer - default: " << umap_cfg_get_bufsize() << " Pages\n"
  << " -c # of churn pages          - default: " << NUMCHURNPAGES << " Pages\n"
  << " -l # of load pages           - default: " << NUMLOADPAGES << " Pages\n"
  << " -t # of churn threads        - default: " << NUMCHURNTHREADS << endl
  << " -r # of load reader threads  - default: " << NUMLOADREADERS << endl
  << " -w # of load writer threads  - default: " << NUMLOADWRITERS << endl
  << " -f [backing file name]       - default: " << FILENAME << endl
  << " -d # seconds to run test     - default: " << TESTDURATION << " seconds\n";
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
  testops.page_buffer_size = umap_cfg_get_bufsize();

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
        if ((testops.num_load_writer_threads = strtoull(optarg, nullptr, 0)) >= 0)
          break;
        goto R0;
      case 'r':
        if ((testops.num_load_reader_threads = strtoull(optarg, nullptr, 0)) >= 0)
          break;
        goto R0;
      case 't':
        if ((testops.num_churn_threads = strtoull(optarg, nullptr, 0)) >= 0)
          break;
        goto R0;

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

  umap_cfg_set_bufsize(testops.page_buffer_size);
}

