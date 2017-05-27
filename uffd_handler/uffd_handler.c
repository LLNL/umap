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
#include <asm/unistd.h>
#include <pthread.h>
#include <poll.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <openssl/sha.h>
#include "uffd_handler.h"

// data structures related to page buffer

#define PAGE_BEGIN(a)   (void *)((uint64_t)a & 0xfffffffffffff000ull);

static pagebuffer_t *pagebuffer;
static bool *pagedirty;
static int startix=0;
// end data structures related to page buffer

int uffd_init(void *region, long pagesize, long num_pages)
{
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
    uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING | 
                                        UFFDIO_REGISTER_MODE_WP;
    fprintf(stdout, "uffdio vals: %x, %d, %ld, %d\n", 
            uffdio_register.range.start, uffd, uffdio_register.range.len, 
            uffdio_register.mode);

    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1) {
        perror("ioctl/uffdio_register");
        exit(1);
    }

    enable_wp_on_pages(uffd, (uint64_t)region, pagesize, num_pages);

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
    pagebuffer = (pagebuffer_t *)calloc(p->bufsize, sizeof(pagebuffer_t));

    for (;;) {
        struct uffd_msg msg;

        struct pollfd pollfd[1];
        pollfd[0].fd = p->uffd;
        pollfd[0].events = POLLIN;

        // wait for a userfaultfd event to occur
        int pollres = poll(pollfd, 1, 2000);

        if (stop_uffd_handler) {
            fprintf(stdout, "%s: Stop seen, exit\n", __FUNCTION__);
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

        if (!pollfd[0].revents & POLLIN)
            continue;

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

        if (msg.event != UFFD_EVENT_PAGEFAULT) {
            printf("Unexpected event %x\n", msg.event);
            continue;
        }
 
        print_uffd_msg_info(&msg);

        //
        // At this point, we know we have had a page fault.  Lets 
        // handle it.
        //
        p->faultnum = p->faultnum + 1;;
        void* addr = (void *)msg.arg.pagefault.address;
        void* page_begin = PAGE_BEGIN(addr);

        //
        // Check to see if the faulting page is alread in memory (needed
        // to detect other potentially stale event messages from other threads
        // in accessing/faulting on this page).
        //
        // TODO(MJM) - Need a better container for pagebuffer.  Using
        // linear search for now...  I think a std::map<page, bufferindex>
        // will work better.
        //
        bool page_in_memory = false;
        int bufidx;
        for (bufidx = 0; bufidx < p->bufsize; bufidx++) {
            if (pagebuffer[bufidx].page == page_begin) {
                page_in_memory = true;
                break;
            }
        }

        if (page_in_memory) {
            if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
                pagebuffer[bufidx].dirty = true;
                disable_wp_on_pages(p->uffd, (uint64_t)page_begin, pagesize, 1);
            }

            struct uffdio_range wake;
            wake.start = (uint64_t)page_begin;
            wake.len = pagesize; 

            if (ioctl(p->uffd, UFFDIO_WAKE, &wake) == -1) {
                perror("ioctl(UFFDIO_WAKE)");
                exit(1);
            }
            continue;
        }

        //
        // We received some sort of fault for page that is not in memory
        //
        if (lseek(p->fd, (off_t)(page_begin - p->base_addr), SEEK_SET) == (off_t)-1) {
            perror("lseek(Read) failed");
            exit(1);
        }

        if (read(p->fd, buf, pagesize) == -1) {
            perror("read failed");
            exit(1);
        }

        if (pagebuffer[startix].page != NULL)
            evict_page(p, &pagebuffer[startix]);

        pagebuffer[startix].page = (void *)page_begin;

        if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
            disable_wp_on_pages(p->uffd, (uint64_t)page_begin, pagesize, 1);
            pagebuffer[startix].dirty = true;
        }
        else {
            enable_wp_on_pages(p->uffd, (uint64_t)page_begin, pagesize, 1);
            pagebuffer[startix].dirty = false;
        }

        startix = (startix +1) % p->bufsize;

        struct uffdio_copy copy;
        copy.src = (uint64_t)buf;
        copy.dst = (uint64_t)page_begin;
        copy.len = pagesize; 
        copy.mode = 0;
        if (ioctl(p->uffd, UFFDIO_COPY, &copy) == -1) {
            perror("ioctl(UFFDIO_COPY)");
            exit(1);
        }
    }
    return NULL;
}

void evict_page(params_t* p, pagebuffer_t* pb)
{
    enable_wp_on_pages(p->uffd, (uint64_t)pb->page, p->pagesize, 1);

    if (pb->dirty) {
        printf("EVICT %016llX Dirty\n", pb->page);
        if (lseek(p->fd, (uint64_t)(pb->page - p->base_addr), SEEK_SET) == -1) {
            perror("lseek(Write) failed");
            assert(0);
        }

        if (write(p->fd, pb->page, p->pagesize)==-1) {
            perror("write failed");
            assert(0);
        }
    }
    else {
        printf("EVICT %016llX Clean\n", pb->page);
    }

    if (madvise(pb->page, p->pagesize, MADV_DONTNEED) == -1) {
        perror("madvise");
        assert(0);
    } 

    pb->page = NULL;
}

void enable_wp_on_pages(
        int uffd, uint64_t start, int64_t size, int64_t pages
)
{
    struct uffdio_writeprotect wp;
    wp.range.start = start;
    wp.range.len = size * pages;
    wp.mode = UFFDIO_WRITEPROTECT_MODE_WP;

    if (ioctl(uffd, UFFDIO_WRITEPROTECT, &wp) == -1) {
        perror("ioctl(UFFDIO_WRITEPROTECT Enable)");
        exit(1);
    }
}

void disable_wp_on_pages(
        int uffd, uint64_t start, int64_t size, int64_t pages
)
{
    struct uffdio_writeprotect wp;
    wp.range.start = start;
    wp.range.len = size * pages;
    wp.mode = 0;

    if (ioctl(uffd, UFFDIO_WRITEPROTECT, &wp) == -1) {
        perror("ioctl(UFFDIO_WRITEPROTECT Disable)");
        exit(1);
    }
}

int uffd_finalize(void *arg, long num_pages)
{
    params_t *p = (params_t *) arg;

    // first write out all modified pages

    int tmpix;
    for (tmpix=0; tmpix < p->bufsize; tmpix++)
        if (pagebuffer[tmpix].page != NULL)
            evict_page(p, &pagebuffer[tmpix]);

    struct uffdio_register uffdio_register;
    uffdio_register.range.start = (unsigned long)p->base_addr;
    uffdio_register.range.len = p->pagesize * num_pages;

    if (ioctl(p->uffd, UFFDIO_UNREGISTER, &uffdio_register.range)) {
        fprintf(stderr, "ioctl unregister failure\n");
        return 1;
    }
    return 0;
}

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

void print_uffd_msg_info(struct uffd_msg* msg)
{
    switch (msg->event) {
        case UFFD_EVENT_PAGEFAULT:  printf("FAULT  "); break;
        case UFFD_EVENT_FORK:       printf("FORK   "); break;
        case UFFD_EVENT_REMAP:      printf("REMAP  "); break;
        case UFFD_EVENT_REMOVE:     printf("REMOVE "); break;
        case UFFD_EVENT_UNMAP:      printf("UNMAP  "); break;
        default:                    printf("?????? "); break;
    }

    printf("%016llX ", msg->arg.pagefault.address);

    if (msg->arg.pagefault.flags == 0)
        printf("READ\n");
    else if (msg->arg.pagefault.flags == UFFD_PAGEFAULT_FLAG_WRITE)
        printf("WRITE\n");
    else if (msg->arg.pagefault.flags == UFFD_PAGEFAULT_FLAG_WP)
        printf("WP\n");
    else 
        printf("WP and WRITE %016llX\n", msg->arg.pagefault.flags);
}

