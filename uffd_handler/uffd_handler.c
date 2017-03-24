// handler of userfaultfd

#include <linux/userfaultfd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
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
#include <openssl/sha.h>

#include "uffd_handler.h"

struct uffdio_register uffdio_register;

long get_pagesize(void)
{
  long ret = sysconf(_SC_PAGESIZE);
  if (ret == -1) {
    perror("sysconf/pagesize");
    exit(1);
  }
  assert(ret > 0);
  return ret;
}

// initializer function
int uffd_init(void *region, long pagesize, long num_pages) {
  // open the userfault fd
  int uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
  if (uffd <  0) {
    perror("userfaultfd syscall not available in this kernel");
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

  struct uffdio_register uffdio_register;
  // register the pages in the region for missing callbacks
  uffdio_register.range.start = (unsigned long)region;
  uffdio_register.range.len = pagesize * num_pages;
  uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
  fprintf(stdout, "uffdio vals: %x, %d, %ld, %d\n", uffdio_register.range.start, uffd, uffdio_register.range.len, uffdio_register.mode);
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
  return uffd;
}


// handler thread
void *uffd_handler(void *arg)
{
  params_t *p = arg;
  long pagesize = p->pagesize;
  char buf[pagesize];

  static void *lastpage = malloc(p->bufsize);
  static unsigned char pagehash = malloc(p->bufsize*SHA_DIGEST_LENGTH);
  static int startix=0;
  static int endix=0;

  p->faultnum=0;

  for (;;) {
    struct uffd_msg msg;

    struct pollfd pollfd[1];
    pollfd[0].fd = p->uffd;
    pollfd[0].events = POLLIN;

    // wait for a userfaultfd event to occur
    int pollres = poll(pollfd, 1, 2000);

    if (stop_uffd_handler){
      fprintf(stdout, "Stop seen, exit handler\n");
      return NULL;
    }

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
 
	p->faultnum = p->faultnum + 1;;
	unsigned long long addr = msg.arg.pagefault.address;
	//fprintf(stderr,"page missed,addr:%x lastpage:%x\n", addr, lastpage);

	unsigned long long page_begin = addr & 0xfffffffffffff000;

#ifdef USEFILE
	lseek(p->fd, (unsigned long) (page_begin - p->base_addr), SEEK_SET);  // reading the same thing
	fprintf(stderr,"file offset is %x \n", (unsigned long) (page_begin - p->base_addr) );
	read(p->fd, buf, pagesize);
#endif
	
	//fprintf(stderr,"page missed,addr:%llx aligned page:%llx\n", addr, page_begin);

	//releasing prev page here results in race condition with multiple app threads
	// ifdef'ed code introduces a 16 element delay buffer

	if (startix==(endix+1) % 16) { // buffer full
#ifdef USEFILE
	  unsigned char tmphash[SHA_DIGEST_LENGTH];
	  SHA1(lastpage[startix], pagesize, tmphash);

	  if (strcmp(tmphash, &pagehash[startix])) { // hashes don't match)
	    lseek(p->fd, (unsigned long) (lastpage[startix] - p->base_addr), SEEK_SET);
	    write(p->fd, lastpage[startix], pagesize);
	  }
 #endif
	  int ret = madvise(lastpage[startix], pagesize, MADV_DONTNEED);
	  if(ret == -1) { perror("madvise"); assert(0); } 
	  startix = (startix + 1) % 16;
	};
	//lastpage[endix]= (void *)addr;
	lastpage[endix]= (void *)page_begin;
	SHA1(page_begin, pagesize, &pagehash[endix*SHA_DIGEST_LENGTH];
	endix = (endix +1) %16;
	
	struct uffdio_copy copy;
	copy.src = (long long)buf;
	//copy.dst = (long long)addr;
	copy.dst = (long long)page_begin;
	copy.len = pagesize; 
	copy.mode = 0;
	if (ioctl(p->uffd, UFFDIO_COPY, &copy) == -1) {
	  perror("ioctl/copy");
	  exit(1);
	}
      }

  }
  return NULL;
}

int uffd_finalize(void *region, int uffd, long pagesize, long num_pages) {
  struct uffdio_register uffdio_register;
  uffdio_register.range.start = (unsigned long)region;
  uffdio_register.range.len = pagesize * num_pages;

  if (ioctl(uffd, UFFDIO_UNREGISTER, &uffdio_register.range)) {
    fprintf(stderr, "ioctl unregister failure\n");
    return 1;
  }

  return 0;
}

