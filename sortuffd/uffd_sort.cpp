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


#define NUMPAGES 10000000
#define NUMTHREADS 2
#define BUFFERSIZE 16

#ifdef _OPENMP
#include <omp.h>
#endif

#define NUMPAGES 10000000
#define NUMTHREADS 2
#define BUFFERSIZE 16

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef UFFD
extern "C" {
#include "../uffd_handler/uffd_handler.h"

volatile int stop_uffd_handler;
}
#endif

static inline uint64_t getns(void)
{
  struct timespec ts;
  int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  assert(ret == 0);
  return (((uint64_t)ts.tv_sec) * 1000000000ULL) + ts.tv_nsec;
}

typedef struct {
  int numpages;
  int numthreads;
  int bufsize;
  char *fn;
} optstruct_t;

optstruct_t options;

void getoptions(optstruct_t *options, int argc, char *argv[]) {

  int c;
  options->numpages = NUMPAGES;
  options->numthreads = NUMTHREADS;
  options->bufsize = BUFFERSIZE;
  options->fn = "/tmp/abc";


  while ((c = getopt(argc, argv, "p:t:f:b:")) != -1) {

    switch(c) {
    case 'p':
      options->numpages = atoi(optarg);
      if (options->numpages > 0)
        break;
      else goto R0;
    case 't':
      options->numthreads = atoi(optarg);
      if (options->numthreads > 0)
        break;
      else goto R0;
    case 'b':
      options->bufsize = atoi(optarg);
      if (options->bufsize > 0)
        break;
      else goto R0;
    case 'f':
      options->fn = optarg;
      break;
    R0:
    default:
      fprintf(stdout, "Usage: %s ",  argv[0]);
      fprintf(stdout, " -p [number of pages], default: %d ", NUMPAGES);	
      fprintf(stdout, " -t [number of threads], default: %d ",  NUMTHREADS);
      fprintf(stdout, " -b [page buffer size], default: %d ",  BUFFERSIZE);
      fprintf(stdout, " -f [file name], name of existing file to read pages from, default no -f\n");
      exit(1);
    }
  }
}

void openandmap(const char *filename, int64_t numbytes, int &fd, void *&region) {

  if( access( filename, W_OK ) != -1 ) {
    remove(filename);
   }
  int open_options = O_RDWR | O_CREAT;
  #ifdef O_LARGEFILE
    open_options != O_LARGEFILE;
  #endif

  fd = open(filename, open_options, S_IRUSR|S_IWUSR);
  if(fd == -1) {
    perror("file open");
    exit(-1);
  }

  if(posix_fallocate(fd,0, numbytes) != 0) {
    perror("Fallocate failed");
  }

  
   // allocate a memory region to be managed by userfaultfd
  region = mmap(NULL, pagesize * num_pages, PROT_READ|PROT_WRITE,
		MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
  if (region == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }
}

void initdata(void *region, uint64_t rlen) {
  int *r = (int *)region;
  std::random_device rd;                                                                                                             std::mt19937 gen(rd());
  std::uniform_int_distribution<uint64_t> rnd_int;
#pragma omp parallel for
  for(int i=0; i< (rlen / sizeof(uint64_t)); ++i) {
    r[i] = rnd_int(gen);;
  }
}

void validatedata(void *region, uint64_t rlen) {
  for(uint64_t i=1; i< (rlen / sizeof(uint64_t)); ++i) {
    if(pint[i] < pint[i-1]) {
      std::cerr << "Worker " << fnum << "found an error!" << std::endl;
      exit(-1);
    }
  }
int main(int argc, char **argv)
{
  int uffd;
  long pagesize;
  int64_t totalbytes;
  pthread_t uffd_thread;

  // parameter block to uffd 
  params_t *p = malloc(sizeof(params_t));

  pagesize = get_pagesize();

  getoptions(&options, argc, argv);

  totalbytes = options.numpages*pagesize;
  openandmap(options.fn, totalbytes, &p->fd,  &p->base_addr);
 
  // start the thread that will handle userfaultfd events

  stop_uffd_handler = 0;

  p->pagesize = pagesize;  

  p->bufsize = options.bufsize;
  p->faultnum = 0;
  p->uffd = uffd_init(p->base_addr, pagesize, options.numpages);

  fprintf(stdout, "%d pages, %d threads\n", options.numpages, options.numthreads);

  pthread_create(&uffd_thread, NULL, uffd_handler, p);

  sleep(1);

  omp_set_num_threads(options.numthreads);

  uint64_t start = getns();
  // init data
  initdata(p->base_addr, totalbytes);
  fprintf(stdout, "Init took %llu\n", getns() - start);
  
  start = getns();
  std::sort(p->base_addr, p->base_addr + (total_bytes/sizeof(uint64_t)));
  fprintf(stdout, "Sort took %llu\n", getns() - start);

  start = getns();
  validatedata(p->base_addr, totalbytes);
  fprintf(stdout, "Validate took %llu\n", getns() - start);
  
  stop_uffd_handler = 1;
  pthread_join(uffd_thread, NULL);
  //fprintf(stdout, "mode %llu\n", (unsigned long long)uffdio_register.mode);
  fprintf(stdout,"total number of fault:%d, value is %d\n",p->faultnum,value);
		
  uffd_finalize(region, p->uffd, pagesize, num_pages);

}

  
