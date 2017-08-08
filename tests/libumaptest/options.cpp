/* This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <iostream>     // cout/cerr
#include <unistd.h>     // getopt()
#include <getopt.h>     // duh...
#include "umaptest.h"
#include "umap.h"

char const* FILENAME = "/tmp/abc";
const uint64_t NUMPAGES = 10000000;
const uint64_t NUMTHREADS = 2;
const uint64_t BUFFERSIZE = 16;
const int SINGLEFILE = -1;

using namespace std;

static void usage(char* pname)
{
  cerr
  << "Usage: " << pname << " [--initonly] [--noinit] [--directio]"
  <<                       " [--usemmap] [-p #] [-t #] [-b #] [-f name]\n\n"
  << " --help                 - This message\n"
  << " --initonly             - Initialize file, then stop\n"
  << " --noinit               - Use previously initialized file\n"
  << " --directio             - Use O_DIRECT for file IO\n"
  << " --usemmap              - Use mmap instead of umap\n"
  << " -p # of pages          - default: " << NUMPAGES << endl
  << " -t # of threads        - default: " << NUMTHREADS << endl
  << " -b page buffer size    - default: " << umap_cfg_get_bufsize() << " Pages\n"
  << " -n number of files     - default: " << -1 << endl
  << " -f [file name]         - backing file name.  Must exist and be correct size for noinit\n";
  exit(1);
}

void umt_getoptions(umt_optstruct_t* testops, int argc, char *argv[])
{
  int c;
  char *pname = argv[0];

  testops->initonly = 0;
  testops->noinit = 0;
  testops->iodirect = 0;
  testops->usemmap = 0;
  testops->numpages = NUMPAGES;
  testops->numthreads = NUMTHREADS;
  testops->bufsize = umap_cfg_get_bufsize();
  testops->fn = FILENAME;
  testops->fnum = SINGLEFILE;

  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
      {"initonly",  no_argument,  &testops->initonly, 1 },
      {"noinit",    no_argument,  &testops->noinit,   1 },
      {"directio",  no_argument,  &testops->iodirect, 1 },
      {"usemmap",   no_argument,  &testops->usemmap,  1 },
      {"help",      no_argument,  NULL,  0 },
      {0,           0,            0,     0 }
    };

    c = getopt_long(argc, argv, "p:t:f:b:n:", long_options, &option_index);
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
      case 't':
        if ((testops->numthreads = strtoull(optarg, nullptr, 0)) > 0)
          break;
        else goto R0;
      case 'b':
        if ((testops->bufsize = strtoull(optarg, nullptr, 0)) > 0)
          break;
        else goto R0;
      case 'n':
        if ((testops->fnum = strtoull(optarg, nullptr, 0)) > 0)
          break;
        else goto R0;
      case 'f':
        testops->fn = optarg;
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

  umap_cfg_set_bufsize(testops->bufsize);
}

long umt_getpagesize(void)
{
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        perror("sysconf/page_size");
        exit(1);
    }
    return page_size;
}

