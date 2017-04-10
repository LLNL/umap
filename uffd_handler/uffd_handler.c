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

// data structures related to page buffer

  typedef struct {
     char sha1hash[SHA_DIGEST_LENGTH];
  } sha1bucket_t;

  static void **pagebuffer;
  static sha1bucket_t *pagehash;
  static int startix=0;


// end data structures related to page buffer

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
  params_t *p = (params_t *) arg;
  long pagesize = p->pagesize;
  char buf[pagesize];

  p->faultnum=0;
  pagebuffer = (void **) calloc(p->bufsize,sizeof(void*)); // allocate and initialize to zero
  pagehash = (sha1bucket_t *) calloc(p->bufsize, sizeof(sha1bucket_t));
  //pagehash = (unsigned char *) malloc(p->bufsize*SHA_DIGEST_LENGTH);
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
	void * addr = (void *)msg.arg.pagefault.address;
	//fprintf(stderr,"page missed,addr:%x pagebuffer:%x\n", addr, pagebuffer[startix]);

	void * page_begin = (void *) ((unsigned long long) addr & 0xfffffffffffff000);

#ifdef USEFILE
	lseek(p->fd, (unsigned long) (page_begin - p->base_addr), SEEK_SET);  // reading the same thing
	//fprintf(stderr,"file offset is %x \n", (unsigned long) (page_begin - p->base_addr) );
	read(p->fd, buf, pagesize);
#else
	memset(buf,'$', pagesize);
#endif
	
	//fprintf(stderr,"page missed,addr:%llx aligned page:%llx\n", addr, page_begin);

	char tmphash[SHA_DIGEST_LENGTH];
	int ret;
	if (pagebuffer[startix] !=NULL)  { // buffer full
#ifdef USEFILE
	  SHA1(pagebuffer[startix], pagesize, tmphash);
	  if (strncmp((const char *)tmphash, (const char *) &pagehash[startix].sha1hash, SHA_DIGEST_LENGTH )) { // hashes don't match)
	    //fprintf(stderr, "Hashes don't match, writing page at addr %llx\n", pagebuffer[startix]);
	    ret = mprotect(pagebuffer[startix], pagesize, PROT_WRITE); // write protect page
	    fprintf(stderr, "Did mprotect on page %llx at offset %llu\n", pagebuffer[startix], (unsigned long) (pagebuffer[startix] - p->base_addr));
	    if(ret == -1) { perror("mprotect"); assert(0); }
	    lseek(p->fd, (unsigned long) (pagebuffer[startix] - p->base_addr), SEEK_SET);
	    write(p->fd, pagebuffer[startix], pagesize);
	  }
 #endif
	  ret = madvise(pagebuffer[startix], pagesize, MADV_DONTNEED);
	  //fprintf(stderr, "base address  %llx, index, %d, effective address %llx\n", pagebuffer, startix, pagebuffer+startix);
	  if(ret == -1) { perror("madvise"); assert(0); } 
	  pagebuffer[startix]=NULL;  // in case later on we unmap more than one page, need to set those slots to zero
	};
	//	pagebuffer[startix]= (void *)addr;
	pagebuffer[startix]= (void *)page_begin;
	SHA1((unsigned char *) buf, pagesize, (unsigned char *) &pagehash[startix].sha1hash);
	startix = (startix +1) % p->bufsize;
	
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

int uffd_finalize(void *arg, long num_pages) {//, int uffd, long pagesize, long num_pages) {
  params_t *p = (params_t *) arg;

#ifdef USEFILE

  // first write out all modified pages

  char tmphash[SHA_DIGEST_LENGTH];
  int tmpix;
  for (tmpix=0; tmpix < p->bufsize; tmpix++) {
    if (pagebuffer[tmpix] !=NULL)  { //has a valid page
      SHA1(pagebuffer[tmpix], p->pagesize, tmphash);
      if (strncmp((const char *)tmphash, (const char *) &pagehash[tmpix].sha1hash, SHA_DIGEST_LENGTH )) { // hashes don't match)
	fprintf(stderr, "Hashes don't match, writing page at addr %llx\n", pagebuffer[tmpix]);
	lseek(p->fd, (unsigned long) (pagebuffer[tmpix] - p->base_addr), SEEK_SET);
	write(p->fd, pagebuffer[tmpix], p->pagesize);
      }
    }
  } 
#endif

  struct uffdio_register uffdio_register;
  uffdio_register.range.start = (unsigned long)p->base_addr;
  uffdio_register.range.len = p->pagesize * num_pages;

  if (ioctl(p->uffd, UFFDIO_UNREGISTER, &uffdio_register.range)) {
    fprintf(stderr, "ioctl unregister failure\n");
    return 1;
  }

  return 0;
}

