/* This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <iostream>     // cout/cerr
#include <unistd.h>     // getopt()
#include <getopt.h>     // duh...
#include "options.h"
#include "umap.h"

static char const* FILENAME = "/tmp/abc";

using namespace std;

static void usage(char* pname)
{
  cerr
  << "Usage: " << pname << " [Options...]\n\n"
  << " --noread                     - Only perform write, not read\n"
  << " --help                       - This message\n"
  << " -f [backing file name]       - default: " << FILENAME << endl;
  exit(1);
}

void getoptions(options_t& testops, int& argc, char **argv)
{
  int c;
  char *pname = argv[0];

  testops.fn=FILENAME;
  testops.noread = 0;

  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
      {"noread",    no_argument,  &testops.noread,  1 },
      {"help",      no_argument,  NULL,  0 },
      {0,           0,            0,     0 }
    };

    c = getopt_long(argc, argv, "f:", long_options, &option_index);
    if (c == -1)
      break;

    switch(c) {
      case 0:
        if (long_options[option_index].flag != 0)
          break;

        usage(pname);
        break;

      case 'f':
        testops.fn = optarg;
        break;

      default:
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

long umt_getpagesize(void)
{
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        perror("sysconf/page_size");
        exit(1);
    }
    return page_size;
}

