#include <random>
#include <algorithm>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>    // optind
#include <errno.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "umap.h"
#include "umaptest.h"

static inline uint64_t getns(void)
{
  struct timespec ts;
  int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  assert(ret == 0);
  return (((uint64_t)ts.tv_sec) * 1000000000ULL) + ts.tv_nsec;
}

void runtest(uint64_t *region, int64_t rlen)
{
#pragma omp parallel
  {
    std::mt19937 gen(omp_get_thread_num());
    std::uniform_int_distribution<uint64_t> rnd_int(0, rlen-1);
    while (1) {
      uint64_t index = rnd_int(gen);
      if (region[index] != index) {
        fprintf(stderr, "%lu != %lu\n", index, region[index]);
        assert(0);
      }
    }
  }
}

void initdata(uint64_t *region, int64_t rlen)
{
  fprintf(stdout, "initdata: %p, %ld\n", region, rlen);
#pragma omp parallel for
  for(int64_t i=0; i < rlen; ++i)
    region[i] = i;
}

int main(int argc, char **argv)
{
  umt_optstruct_t options;
  long pagesize;
  int64_t totalbytes;
  pthread_t uffd_thread;
  uint64_t arraysize;
  params_t *p = (params_t *) malloc(sizeof(params_t));

  pagesize = get_pagesize();

  umt_getoptions(options, argc, argv);

  totalbytes = options.numpages*pagesize;
  umt_openandmap(options, totalbytes, p->fd,  p->base_addr);
 
  if ( ! options.usemmap ) {
    fprintf(stdout, "Using UserfaultHandler Buffer\n");
    p->pagesize = pagesize;  
    p->bufsize = options.bufsize;
    p->faultnum = 0;
    p->uffd = uffd_init(p->base_addr, pagesize, options.numpages);

    pthread_create(&uffd_thread, NULL, uffd_handler, p);
    sleep(1);
  }
  else {
    fprintf(stdout, "Using vanilla mmap()\n");
  }

  fprintf(stdout, "%lu pages, %lu threads\n", options.numpages, options.numthreads);

  omp_set_num_threads(options.numthreads);

  uint64_t *arr = (uint64_t *) p->base_addr; 
  arraysize = totalbytes/sizeof(int64_t);

  uint64_t start = getns();
  if ( !options.noinit ) {
    // init data
    initdata(arr, arraysize);
    fprintf(stdout, "Init took %f us\n", (double)(getns() - start)/1000000.0);
  }

  if ( !options.initonly ) {
    start = getns();
    runtest(arr, arraysize);
    fprintf(stdout, "Sort took %f us\n", (double)(getns() - start)/1000000.0);
  }
  
  if ( ! options.usemmap ) {
    stop_umap_handler();
    pthread_join(uffd_thread, NULL);
    uffd_finalize(p, options.numpages);
  }

  return 0;
}
