#include <linux/userfaultfd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
//#include <sys/time.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#ifdef _OPENMP
#include <omp.h>
#endif

static volatile int stop;

struct params {
  int uffd;
  long page_size;
#ifdef USEFILE
  int fd;
#endif
};
int faultnum;

static inline uint64_t getns(void)
{
    struct timespec ts;
    int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    assert(ret == 0);
    return (((uint64_t)ts.tv_sec) * 1000000000ULL) + ts.tv_nsec;
}

/* ouble get_wtime() */
/* { */
/*     struct timeval now; */
/*     gettimeofday(&now, NULL); */
/*     return double(now.tv_sec) + double(now.tv_usec)/1e6; */
/* } */

static long get_page_size(void)
{
    long ret = sysconf(_SC_PAGESIZE);
    if (ret == -1) {
        perror("sysconf/pagesize");
        exit(1);
    }
    assert(ret > 0);
    return ret;
}

static void *handler(void *arg)
{
    struct params *p = arg;
    long page_size = p->page_size;
    char buf[page_size];

#ifdef TESTBUFFER
    static void *lastpage[16];
    static int startix=0;
    static int endix=0;
#endif

    faultnum=0;

    for (;;) {
        struct uffd_msg msg;

        struct pollfd pollfd[1];
        pollfd[0].fd = p->uffd;
        pollfd[0].events = POLLIN;

        // wait for a userfaultfd event to occur
        int pollres = poll(pollfd, 1, 2000);

        if (stop)
            return NULL;

        switch (pollres) {
        case -1:
            perror("poll/userfaultfd");
            continue;
        case 0:
            continue;
        case 1:
            break;
        default:
            fprintf(stderr, "unexpected poll result\n");
            exit(1);
        }

        if (pollfd[0].revents & POLLERR) {
            fprintf(stderr, "pollerr\n");
            exit(1);
        }

        if (!pollfd[0].revents & POLLIN) {
            continue;
        }

        int readres = read(p->uffd, &msg, sizeof(msg));
        if (readres == -1) {
            if (errno == EAGAIN)
                continue;
            perror("read/userfaultfd");
            exit(1);
        }

        if (readres != sizeof(msg)) {
            fprintf(stderr, "invalid msg size\n");
            exit(1);
        }

        // handle the page fault by copying a page worth of bytes
        if (msg.event & UFFD_EVENT_PAGEFAULT)
        {
 
	    faultnum++;
            unsigned long long addr = msg.arg.pagefault.address;
            //fprintf(stderr,"page missed,addr:%x lastpage:%x\n", addr, lastpage);

	    unsigned long long page_begin = addr & 0xfffffffffffff000;

	    //fprintf(stderr,"page missed,addr:%llx aligned page:%llx\n", addr, page_begin);

	    //releasing prev page here results in race condition with multiple app threads
	    // ifdef'ed code introduces a 16 element delay buffer

#ifdef TESTBUFFER
	    if (startix==(endix+1) % 16) { // buffer full
	      int ret = madvise(lastpage[startix], page_size, MADV_DONTNEED);
	      if(ret == -1) { perror("madvise"); assert(0); } 
	      startix = (startix + 1) % 16;
	    };
	    //lastpage[endix]= (void *)addr;
	    lastpage[endix]= (void *)page_begin;
	    endix = (endix +1) %16;
#endif

#ifdef USEFILE
	    lseek(p->fd, 0, SEEK_SET);
	    read(p->fd, buf, 10);
#endif
            struct uffdio_copy copy;
            copy.src = (long long)buf;
            //copy.dst = (long long)addr;
	    copy.dst = (long long)page_begin;
	    copy.len = page_size; 
            copy.mode = 0;
            if (ioctl(p->uffd, UFFDIO_COPY, &copy) == -1) {
                perror("ioctl/copy");
                exit(1);
            }
        }
        //printf("number of fault:%d\n",faultnum);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    int uffd;
    long page_size;
    long num_pages;
    void *region;
    pthread_t uffd_thread;

    page_size = get_page_size();
    num_pages = 10000000;

    // open the userfault fd
    uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
    if (uffd == -1) {
        perror("syscall/userfaultfd");
        exit(1);
    }

    // enable for api version and check features
    struct uffdio_api uffdio_api;
    uffdio_api.api = UFFD_API;
    uffdio_api.features = 0;
    if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1) {
        perror("ioctl/uffdio_api");
        exit(1);
    }

    if (uffdio_api.api != UFFD_API) {
        fprintf(stderr, "unsupported userfaultfd api\n");
        exit(1);
    }
    fprintf(stdout, "Feature bitmap %llx\n", uffdio_api.features);

    // allocate a memory region to be managed by userfaultfd
    region = mmap(NULL, page_size * num_pages, PROT_READ|PROT_WRITE,
		  MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
    //MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // register the pages in the region for missing callbacks
    struct uffdio_register uffdio_register;
    uffdio_register.range.start = (unsigned long)region;
    uffdio_register.range.len = page_size * num_pages;
    uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1) {
        perror("ioctl/uffdio_register");
        exit(1);
    }

    if ((uffdio_register.ioctls & UFFD_API_RANGE_IOCTLS) !=
            UFFD_API_RANGE_IOCTLS) {
        fprintf(stderr, "unexpected userfaultfd ioctl set\n");
        exit(1);
    }

    fprintf(stdout, "mode %llu\n", (unsigned long long)uffdio_register.mode);
    // start the thread that will handle userfaultfd events



    stop = 0;

    struct params p;
    p.uffd = uffd;
    p.page_size = page_size;
#ifdef USEFILE
    fprintf(stdout, "USEFILE enabled %s\n", "/tmp/notes.txt");
    p.fd = open("/tmp/notes.txt", O_RDONLY);// | O_DIRECT);
    if (p.fd == -1) {
      perror("file open");
      exit(1);
    }
#endif
    pthread_create(&uffd_thread, NULL, handler, &p);
    //printf("total number of fault:%d\n",faultnum);
    sleep(1);

    // storage for the latencies for each page
    int num_batches = 10;
    uint64_t *latencies = malloc(sizeof(uint64_t) * num_batches);
    assert(latencies);
    memset(latencies, 0, sizeof(uint64_t) * num_batches);

    // measure latency in batches
    long batch_size=num_pages/num_batches;  

#ifdef TESTBUFFER
    fprintf(stdout, "TESTBUFFER enabled\n");
#endif

    // touch each page in the region
    int value;
    int *cur = region;

#pragma omp parallel for reduction(+:value) //private (value)
    for (long i = 0; i < num_pages; i+=batch_size) {
      uint64_t start = getns();
      for (long j=0;j<batch_size;j++)
    	{
    	  //fprintf(stdout, "mode %llu\n", (unsigned long long)uffdio_register.mode);
    	  //fprintf(stdout, "cur %x, adddress at i:%d, j:%d, %x\n", cur, i, j, &cur[i*1024 + j*1024]);
    	  int v = cur[i*1024 + j*1024 + 5];
    	  //int v = *cur;
    	  //fprintf(stdout, "%llu\n", (unsigned long long)latencies[i]);
    	  //fprintf(stdout, "mode %llu\n", (unsigned long long)uffdio_register.mode);
    	  //fprintf(stdout,"%d\n",faultnum);
    	  value += v;
	  
#ifndef TESTBUFFER
	  int ret = madvise(cur+(i*1024 + j*1024), page_size, MADV_DONTNEED);
	  if(ret == -1) { perror("madvise"); assert(0); } 
#endif

        }
        uint64_t dur = getns() - start;
        latencies[i/batch_size] = dur/batch_size;
        //fprintf(stdout, "%llu\n", (unsigned long long)latencies[i]);
    }
//------------------check when page is loaded----------------------

/* #pragma omp parallel for private (value) */
/*     for (long i = 0; i < num_pages; i+=1) { */
/*         uint64_t start = getns(); */
/*         //fprintf(stdout, "mode %llu\n", (unsigned long long)uffdio_register.mode); */
/* 	//        int v = *((int*)cur); */
/*         int v = cur[i*1024]; */
/*         uint64_t dur = getns() - start; */
/*         latencies[i] = dur; */
/*         //fprintf(stdout, "%llu\n", (unsigned long long)latencies[i]); */
/*         //fprintf(stdout, "mode %llu\n", (unsigned long long)uffdio_register.mode); */
/*         //fprintf(stdout,"%d\n",faultnum); */
/*         value += v; */
/* 	int ret = madvise(&cur[i*1024], page_size, MADV_DONTNEED); */
/* 	if(ret == -1) { perror("madvise"); assert(0); } */
/*         //cur += page_size; */
/*     } */
    //-------------------------------------------------------------------
    stop = 1;
    pthread_join(uffd_thread, NULL);
    //fprintf(stdout, "mode %llu\n", (unsigned long long)uffdio_register.mode);
    fprintf(stdout,"total number of fault:%d, value is %d\n",faultnum,value);

    if (ioctl(uffd, UFFDIO_UNREGISTER, &uffdio_register.range)) {
        fprintf(stderr, "ioctl unregister failure\n");
        return 1;
    }

    for (long i = 0; i < num_batches; i++) {
        fprintf(stdout, "%llu\n", (unsigned long long)latencies[i]);
    }

    free(latencies);
    munmap(region, page_size * num_pages);

    return 0;
}
