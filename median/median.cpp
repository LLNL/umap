
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

extern "C" {
#include "../uffd_handler/uffd_handler.h"

    volatile int stop_uffd_handler;
}


#define NUMPAGES 10000000
#define NUMTHREADS 2
#define BUFFERSIZE 16

#ifdef _OPENMP
#include <omp.h>
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
  options->fn = (char *) "/tmp/abc";


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


void initdata(uint64_t *region, int64_t rlen) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> rnd_int;
#pragma omp parallel for
    for(int i=0; i< rlen; ++i) {
        //region[i] = (uint64_t) (rlen - i);// rnd_int(gen);
        region[i] = rnd_int(gen)>>1;//divide all values by 2 because of overflow in torben
        //printf("%lld\n", (long long)region[i]);
    }
}
void openandmap(const char *filename, int64_t numbytes, int &fd, void *&region) {
    struct stat file_s;

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

    region = mmap(NULL, numbytes, PROT_READ|PROT_WRITE,
                  MAP_SHARED, fd, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
}
uint64_t torben(uint64_t *m, int n)
{
    int         i, less, greater, equal;
    uint64_t  min, max, guess, maxltguess, mingtguess;

    min = max = m[0] ;
    for (i=1 ; i<n ; i++) {
        if (m[i]<min) min=m[i];
        if (m[i]>max) max=m[i];
        //fprintf(stdout,"m:%llu\n",m[i]);
    }
    //fprintf(stdout,"Max:%llu\nMin:%llu\n",max,min);

    while (1) {
        guess = (min+max)/2;
        less = 0; greater = 0; equal = 0;
        maxltguess = min ;
        mingtguess = max ;
        for (i=0; i<n; i++) {
            if (m[i]<guess) {
                less++;
                if (m[i]>maxltguess) maxltguess = m[i] ;
            } else if (m[i]>guess) {
                greater++;
                if (m[i]<mingtguess) mingtguess = m[i] ;
            } else equal++;
        }
        if (less <= (n+1)/2 && greater <= (n+1)/2) break ;
        else if (less>greater) max = maxltguess ;
        else min = mingtguess;
        //fprintf(stdout,"guess: %llu less:%d greater:%d\n",guess,less,greater);
    }
    if (less >= (n+1)/2) return maxltguess;
    else if (less+equal >= (n+1)/2) return guess;
    else return mingtguess;
}

int main(int argc, char **argv)
{
  int uffd;
  long pagesize;
  int64_t totalbytes;
  pthread_t uffd_thread;
  int64_t arraysize;
  uint64_t median;
  int fd;
  void *base_addr;
  // parameter block to uffd 
  //params_t *p = (params_t *) malloc(sizeof(params_t));

  pagesize = get_pagesize();

  getoptions(&options, argc, argv);

  totalbytes = options.numpages*pagesize;
  openandmap(options.fn, totalbytes, fd,base_addr);

  fprintf(stdout, "%d pages, %d threads\n", options.numpages, options.numthreads);

  omp_set_num_threads(options.numthreads);

  uint64_t *arr = (uint64_t *) base_addr;
  arraysize = totalbytes/sizeof(int64_t);
  fprintf(stdout,"Array size: %lld\n",arraysize);

  uint64_t start = getns();
  // init data
  initdata(arr, arraysize);
  fprintf(stdout, "Init took %f us\n", (double)(getns() - start)/1000000.0);

  start = getns();
  median=torben(arr,arraysize);
  fprintf(stdout, "Median is %llu, Find median took %f us\n",median,(double)(getns() - start)/1000000.0);
}

