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
#include "umap.h"

// data structures related to page buffer
static pagebuffer_t* pagebuffer;
static bool* pagedirty;
static int startix=0;

#ifdef ENABLE_FAULT_TRACE_BUFFER
static page_activity_trace_t* trace_buf;
static int trace_bufsize = 1000;
static int trace_idx = 0;
static int trace_seq = 1;
#endif // ENABLE_FAULT_TRACE_BUFFER

// end data structures related to page buffer

int uffd_init(void* region, long pagesize, long num_pages)
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
    uffdio_register.range.start = (uint64_t)region;
    uffdio_register.range.len = pagesize * num_pages;
    uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING | 
                                        UFFDIO_REGISTER_MODE_WP;
    fprintf(stdout, "uffdio region=%p - %p\n", 
            region, 
            (void*)(uffdio_register.range.start+uffdio_register.range.len));

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
    return uffd;
}

void *uffd_handler(void *arg)
{
    params_t *p = (params_t *) arg;
    long pagesize = p->pagesize;
    char buf[pagesize];

    p->faultnum=0;
    pagebuffer = (pagebuffer_t *)calloc(p->bufsize, sizeof(pagebuffer_t));
#ifdef ENABLE_FAULT_TRACE_BUFFER
    trace_buf = (page_activity_trace_t *)calloc(trace_bufsize, sizeof(*trace_buf));
#endif // ENABLE_FAULT_TRACE_BUFFER

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
 
        //
        // At this point, we know we have had a page fault.  Let's handle it.
        //
#define PAGE_BEGIN(a)   (void*)((uint64_t)a & ~(pagesize-1));

        p->faultnum = p->faultnum + 1;;
        void* fault_addr = (void*)msg.arg.pagefault.address;
        void* page_begin = PAGE_BEGIN(fault_addr);

#ifdef ENABLE_FAULT_TRACE_BUFFER
        if (msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WP) {
            TRACE(page_begin, ft_wp, et_NA);
        }
        else if (msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WRITE) {
            TRACE(page_begin, ft_write, et_NA);
        }
        else {
            TRACE(page_begin, ft_read, et_NA);
        }
#endif // ENABLE_FAULT_TRACE_BUFFER

        //
        // Check to see if the faulting page is already in memory. This can
        // happen if more than one thread causes a fault for the same page.
        //
        // TODO(MJM) - Implement better container to get rid of linear
        // search.
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
            if (msg.arg.pagefault.flags & 
                    (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
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
        // Page not in memory, read it in and evict someone
        //
        if (lseek(p->fd, (off_t)((uint64_t)page_begin - (uint64_t)p->base_addr), SEEK_SET) == (off_t)-1) {
            perror("lseek(Read) failed");
            exit(1);
        }

        if (read(p->fd, buf, pagesize) == -1) {
            perror("read failed");
            exit(1);
        }

        if (pagebuffer[startix].page)
            evict_page(p, &pagebuffer[startix]);

        pagebuffer[startix].page = page_begin;

        if (msg.arg.pagefault.flags & 
                (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
            disable_wp_on_pages(p->uffd, (uint64_t)page_begin, pagesize, 1);
            pagebuffer[startix].dirty = true;
        }
        else {
            pagebuffer[startix].dirty = false;
        }
        startix = (startix +1) % p->bufsize;

        struct uffdio_copy copy;
        copy.src = (uint64_t)buf;
        copy.dst = (uint64_t)page_begin;
        copy.len = pagesize; 
        if (msg.arg.pagefault.flags & 
                (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
            copy.mode = 0;
            if (ioctl(p->uffd, UFFDIO_COPY, &copy) == -1) {
                perror("ioctl(UFFDIO_COPY wake)");
                exit(1);
            }
        }
        else {
            copy.mode = UFFDIO_COPY_MODE_DONTWAKE;
            if (ioctl(p->uffd, UFFDIO_COPY, &copy) == -1) {
                perror("ioctl(UFFDIO_COPY nowake)");
                exit(1);
            }

            // Enable_wp will wake up thread
            enable_wp_on_pages(p->uffd, (uint64_t)page_begin, pagesize, 1);
        }
    }
    return NULL;
}

void evict_page(params_t* p, pagebuffer_t* pb)
{
    //
    // The implementation of UFFDIO_WRITEPROTET always wakes up thread
    // sleeping on this page if UFFDIO_WRITEPROTECT_MODE_WP is set.
    //
    // Further, UFFDIO_WRITEPROTECT_MODE_DONTWAKE is NOT allowed
    // for UFFDIO_WRITEPROTECT calls if UFFDIO_WRITEPROCT_MODE_WP is
    // set.  In other words, the only time you can disabling WAKE is
    // when you are disabling write protect.
    //
    enable_wp_on_pages(p->uffd, (uint64_t)pb->page, p->pagesize, 1);

    if (pb->dirty) {
        TRACE(pb->page, ft_NA, et_dirty);
        if (lseek(p->fd, ((uint64_t)pb->page - (uint64_t)p->base_addr), SEEK_SET) == -1) {
            perror("lseek(Write) failed");
            assert(0);
        }

        if (write(p->fd, (void*)(pb->page), p->pagesize)==-1) {
            perror("write failed");
            assert(0);
        }
    }
    else {
        TRACE(pb->page, ft_NA, et_clean);
    }

    if (madvise((void*)(pb->page), p->pagesize, MADV_DONTNEED) == -1) {
        perror("madvise");
        assert(0);
    } 

    pb->page = 0ull;
}

void enable_wp_on_pages(int uffd, uint64_t start, int64_t size, int64_t pages)
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

void disable_wp_on_pages(int uffd, uint64_t start, int64_t size, int64_t pages)
{
    struct uffdio_writeprotect wp;
    wp.range.start = start;
    wp.range.len = size * pages;
    wp.mode = UFFDIO_WRITEPROTECT_MODE_DONTWAKE;

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
        if (pagebuffer[tmpix].page)
            evict_page(p, &pagebuffer[tmpix]);

    struct uffdio_register uffdio_register;
    uffdio_register.range.start = (uint64_t)p->base_addr;
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

#ifdef ENABLE_FAULT_TRACE_BUFFER
void pa_trace(uint64_t page, enum fault_types ftype, enum evict_types etype)
{
    trace_buf[trace_idx].trace_seq = trace_seq++;
    trace_buf[trace_idx].page = (void*)page;
    trace_buf[trace_idx].ftype = ftype;
    trace_buf[trace_idx].etype = etype;

    trace_idx = (trace_idx +1) % trace_bufsize;
}
#endif // ENABLE_FAULT_TRACE_BUFFER
