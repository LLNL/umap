#include <iostream>     // cout/cerr
#include <unistd.h>     // getopt()
#include <getopt.h>
#include "umaptest.h"

char const* FILENAME = "/tmp/abc";
const uint64_t NUMPAGES = 10000000;
const uint64_t NUMTHREADS = 2;
const uint64_t BUFFERSIZE = 16;

using namespace std;

static void usage(char* pname)
{
  cerr
  << "Usage: " << pname << " [--initonly] [--noinit] [--directio]"
  <<                       " [--UseMmap] [-p #] [-t #] [-b #] [-f name]\n\n"
  << " --help                 - This message\n"
  << " --initonly             - Initialize file, then stop\n"
  << " --noinit               - Use previously initialized file\n"
  << " --directio             - Use O_DIRECT for file IO\n"
  << " --usemmap              - Use mmap instead of umap\n"
  << " -p # of pages          - default: " << NUMPAGES << endl
  << " -t # of threads        - default: " << NUMTHREADS << endl
  << " -b page buffer size    - default: " << BUFFERSIZE << endl
  << " -f [file name]         - backing file name.  Must exist for NoInit\n";
  exit(1);
}

void umt_getoptions(umt_optstruct_t& testops, int argc, char *argv[])
{
  int c;
  char *pname = argv[0];

  testops = (umt_optstruct_t) { .initonly = 0, .noinit = 0, .iodirect = 0, 
                                .usemmap = 0, .numpages = NUMPAGES, 
                                .numthreads = NUMTHREADS, 
                                .bufsize = BUFFERSIZE, .fn = FILENAME};
  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
      {"initonly",  no_argument,  &testops.initonly, 1 },
      {"noinit",    no_argument,  &testops.noinit,   1 },
      {"directio",  no_argument,  &testops.iodirect, 1 },
      {"usemmap",   no_argument,  &testops.usemmap,  1 },
      {"help",      no_argument,  NULL,  0 },
      {0,           0,            0,     0 }
    };

    c = getopt_long(argc, argv, "p:t:f:b:", long_options, &option_index);
    if (c == -1)
      break;

    switch(c) {
      case 0:
        if (long_options[option_index].flag != 0)
          break;

        usage(pname);
        break;

      case 'p':
        if ((testops.numpages = strtoull(optarg, nullptr, 0)) > 0)
          break;
        goto R0;
      case 't':
        if ((testops.numthreads = strtoull(optarg, nullptr, 0)) > 0)
          break;
        else goto R0;
      case 'b':
        if ((testops.bufsize = strtoull(optarg, nullptr, 0)) > 0)
          break;
        else goto R0;
      case 'f':
        testops.fn = optarg;
        break;
      default:
      R0:
        usage(pname);
    }
  }
}
