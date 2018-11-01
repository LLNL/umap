/* This file is part of UMAP.  For copyright information see the COPYRIGHT
 * file in the top level directory, or at
 * https://github.com/LLNL/umap/blob/master/COPYRIGHT. This program is free
 * software; you can redistribute it and/or modify it under the terms of the
 * GNU Lesser General Public License (as published by the Free Software
 * Foundation) version 2.1 dated February 1999.  This program is distributed in
 * the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the terms and conditions of the GNU Lesser General Public License for
 * more details.  You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <iostream>
#include <string>
#include <sstream>
#include <cassert>
#include <random>
#include <string>
#include <vector>
#include <parallel/algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <omp.h>

#include "umap/umap.h"
#include "../utility/commandline.hpp"
#include "../utility/umap_file.hpp"

using namespace std;

bool sort_ascending = true;

static inline uint64_t getns(void)
{
  struct timespec ts;
  int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  assert(ret == 0);
  return (((uint64_t)ts.tv_sec) * 1000000000ULL) + ts.tv_nsec;
}

void initdata(uint64_t *region, uint64_t rlen) {
  fprintf(stdout, "initdata: %p, %llu\n", region, rlen);
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint64_t> rnd_int;
#pragma omp parallel for
  for(uint64_t i=0; i < rlen; ++i) {
    region[i] = (uint64_t) (rlen - i);
  }
}

void validatedata(uint64_t *region, uint64_t rlen) {
  if (sort_ascending == true) {
#pragma omp parallel for
    for(uint64_t i = 0; i < rlen; ++i) {
        if (region[i] != (i+1)) {
            fprintf(stderr, "Worker %d found an error at index %lu, %lu != lt %lu!\n",
                            omp_get_thread_num(), i, region[i], i+1);

            if (i < 3) {
                fprintf(stderr, "Context ");
                for (int j=0; j < 7; j++) {
                    fprintf(stderr, "%lu ", region[j]);
                }
                fprintf(stderr, "\n");
            }
            else if (i > (rlen-4)) {
                fprintf(stderr, "Context ");
                for (uint64_t j=rlen-8; j < rlen; j++) {
                    fprintf(stderr, "%lu ", region[j]);
                }
                fprintf(stderr, "\n");
            }
            else {
                fprintf(stderr,
                    "Context i-3 i-2 i-1 i i+1 i+2 i+3:%lu %lu %lu %lu %lu %lu %lu\n",
                    region[i-3], region[i-2], region[i-1], region[i], region[i+1], region[i+2], region[i+3]);
            }
        }
    }
  }
  else {
#pragma omp parallel for
    for(uint64_t i = 0; i < rlen; ++i) {
        if(region[i] != (rlen - i)) {
            fprintf(stderr, "Worker %d found an error at index %lu, %lu != %lu!\n",
                            omp_get_thread_num(), i, region[i], (rlen - i));

            if (i < 3) {
                fprintf(stderr, "Context ");
                for (int j=0; j < 7; j++) {
                    fprintf(stderr, "%lu ", region[j]);
                }
                fprintf(stderr, "\n");
            }
            else if (i > (rlen-4)) {
                fprintf(stderr, "Context ");
                for (uint64_t j=rlen-8; j < rlen; j++) {
                    fprintf(stderr, "%lu ", region[j]);
                }
                fprintf(stderr, "\n");
            }
            else {
                fprintf(stderr,
                    "Context i-3 i-2 i-1 i i+1 i+2 i+3:%lu %lu %lu %lu %lu %lu %lu\n",
                    region[i-3], region[i-2], region[i-1], region[i], region[i+1], region[i+2], region[i+3]);
            }
        }
    }
  }
}

void* map_in_file(const utility::umt_optstruct_t* testops, uint64_t numbytes)
{
  void* region = NULL;
  int open_options = O_RDWR | O_LARGEFILE | O_DIRECT;
  int fd;
  string filename(testops->filename);

  if ( testops->initonly || !testops->noinit ) {
    open_options |= O_CREAT;
    unlink(filename.c_str());   // Remove the file if it exists
  }

  if ( ( fd = open(filename.c_str(), open_options, S_IRUSR | S_IWUSR) ) == -1 ) {
    string estr = "Failed to open/create " + filename + ": ";
    perror(estr.c_str());
    return NULL;
  }

  if ( open_options & O_CREAT ) { // If we are initializing, attempt to pre-allocate disk space for the file.
    try {
      int x;
      if ( ( x = posix_fallocate(fd, 0, numbytes) != 0 ) ) {
        ostringstream ss;
        ss << "Failed to pre-allocate " << numbytes << " bytes in " << filename << ": ";
        perror(ss.str().c_str());
        return NULL;
      }
    } catch(const std::exception& e) {
      cerr << "posix_fallocate: " << e.what() << endl;
      return NULL;
    } catch(...) {
      cerr << "posix_fallocate failed to allocate backing store\n";
      return NULL;
    }
  }

  struct stat sbuf;
  if (fstat(fd, &sbuf) == -1) {
    string estr = "Failed to get status (fstat) for " + filename + ": ";
    perror(estr.c_str());
    return NULL;
  }

  if ( (off_t)sbuf.st_size != (numbytes) ) {
    cerr << filename << " size " << sbuf.st_size << " does not match specified data size of " << (numbytes) << endl;
    return NULL;
  }

  const int prot = PROT_READ|PROT_WRITE;

  if ( testops->usemmap ) {
    region = mmap(NULL, numbytes, prot, MAP_SHARED | MAP_NORESERVE, fd, 0);

    if (region == MAP_FAILED) {
      ostringstream ss;
      ss << "mmap of " << numbytes << " bytes failed for " << filename << ": ";
      perror(ss.str().c_str());
      return NULL;
    }
  }
  else {
    int flags = UMAP_PRIVATE;

    region = umap(NULL, numbytes, prot, flags, fd, 0);
    if ( region == UMAP_FAILED ) {
        ostringstream ss;
        ss << "umap_mf of " << numbytes << " bytes failed for " << filename << ": ";
        perror(ss.str().c_str());
        return NULL;
    }
  }

  return region;
}

void unmap_file(const utility::umt_optstruct_t* testops, uint64_t numbytes, void* region)
{
  if ( testops->usemmap ) {
    if ( munmap(region, numbytes) < 0 ) {
      ostringstream ss;
      ss << "munmap failure: ";
      perror(ss.str().c_str());
      exit(-1);
    }
  }
  else {
    if (uunmap(region, numbytes) < 0) {
      ostringstream ss;
      ss << "uunmap of failure: ";
      perror(ss.str().c_str());
      exit(-1);
    }
  }
}

int main(int argc, char **argv)
{
  utility::umt_optstruct_t options;
  uint64_t pagesize;
  uint64_t totalbytes;
  uint64_t arraysize;
  void* base_addr;

  uint64_t start = getns();
  pagesize = (uint64_t)utility::umt_getpagesize();

  umt_getoptions(&options, argc, argv);

  omp_set_num_threads(options.numthreads);

  totalbytes = options.numpages*pagesize;
  base_addr = utility::map_in_file(options.filename, options.initonly, options.noinit, options.usemmap, totalbytes);
  if (base_addr == nullptr)
    return -1;

  fprintf(stdout, "umap INIT took %f seconds\n", (double)(getns() - start)/1000000000.0);
  fprintf(stdout, "%lu pages, %llu bytes, %lu threads\n", options.numpages, totalbytes, options.numthreads);

  uint64_t *arr = (uint64_t *) base_addr;
  arraysize = totalbytes/sizeof(uint64_t);

  start = getns();
  if ( !options.noinit ) {
    // init data
    initdata(arr, arraysize);
    fprintf(stdout, "Init took %f seconds\n", (double)(getns() - start)/1000000000.0);
  }

  if ( !options.initonly )
  {
    start = getns();
    sort_ascending = (arr[0] != 1);

    if (sort_ascending == true) {
      printf("Sorting in Ascending Order\n");
      __gnu_parallel::sort(arr, &arr[arraysize], std::less<uint64_t>(), __gnu_parallel::quicksort_tag());
    }
    else {
      printf("Sorting in Descending Order\n");
      __gnu_parallel::sort(arr, &arr[arraysize], std::greater<uint64_t>(), __gnu_parallel::quicksort_tag());
    }

    fprintf(stdout, "Sort took %f seconds\n", (double)(getns() - start)/1000000000.0);

    start = getns();
    validatedata(arr, arraysize);
    fprintf(stdout, "Validate took %f seconds\n", (double)(getns() - start)/1000000000.0);
  }

  start = getns();
  utility::unmap_file(options.usemmap, totalbytes, base_addr);
  fprintf(stdout, "umap TERM took %f seconds\n", (double)(getns() - start)/1000000000.0);

  return 0;
}
