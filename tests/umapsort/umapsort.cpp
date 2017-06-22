// uffd sort benchmark

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

void initdata(uint64_t *region, int64_t rlen) {
  fprintf(stdout, "initdata: %p, %ld\n", region, rlen);
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint64_t> rnd_int;
#pragma omp parallel for
  for(int i=0; i< rlen; ++i) {
    region[i] = (uint64_t) (rlen - i);// rnd_int(gen);
    //region[i] = rnd_int(gen);
  }
}

void validatedata(uint64_t *region, uint64_t rlen) {
#pragma omp parallel for
    for(uint64_t i=1; i< rlen; ++i) {
        if(region[i] < region[i-1]) {
            fprintf(stderr, "Worker %d found an error at index %lu, %lu is lt %lu!\n", 
                            omp_get_thread_num(), i, region[i], region[i-1]);

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

int main(int argc, char **argv)
{
  umt_optstruct_t options;
  long pagesize;
  int64_t totalbytes;
  pthread_t uffd_thread;
  uint64_t arraysize;
  // parameter block to uffd 
  params_t *p = (params_t *) malloc(sizeof(params_t));

  pagesize = get_pagesize();

  umt_getoptions(options, argc, argv);

  totalbytes = options.numpages*pagesize;
  umt_openandmap(options, totalbytes, p->fd,  p->base_addr);
 
  if ( ! options.usemmap ) {
    fprintf(stdout, "Using UserfaultHandler Buffer\n");

    // start the thread that will handle userfaultfd events
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
    std::sort(arr, &arr[arraysize]);
    fprintf(stdout, "Sort took %f us\n", (double)(getns() - start)/1000000.0);

    start = getns();
    validatedata(arr, arraysize);
    fprintf(stdout, "Validate took %f us\n", (double)(getns() - start)/1000000.0);
  }
  
  if ( ! options.usemmap ) {
    stop_umap_handler();
    pthread_join(uffd_thread, NULL);
    uffd_finalize(p, options.numpages);
  }

  return 0;
}
