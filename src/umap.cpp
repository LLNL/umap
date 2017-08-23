/* This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <iostream>
#include <cstdint>
#include <vector>
#include <thread>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>              // open/close
#include <unistd.h>             // sysconf()
#include <sys/syscall.h>        // syscall()
#include <sys/mman.h>           // mmap()
#include <poll.h>               // poll()
#include <assert.h>
#include <linux/userfaultfd.h>
#include <utmpx.h>              // sched_getcpu()
#include <signal.h>
#include <cstring>
#include "umap.h"               // API to library
#include "umaplog.h"            // umap_log()

using namespace std;

const int UMAP_VERSION_MAJOR = 0;
const int UMAP_VERSION_MINOR = 0;
const int UMAP_VERSION_PATCH = 1;
const int UMAP_DEFAULT_PBSIZE = 16;
static int umap_page_bufsize = UMAP_DEFAULT_PBSIZE;
static long page_size;

class umap_page;
class _umap {
    public:
        _umap(void* _mmap_addr, size_t _mmap_length, int num_backing_file, umap_backing_file* backing_files);
        void uffd_finalize(void);

        bool is_in_umap(const void* page_begin) {
            return page_begin >= segment_address && page_begin < (void*)((uint64_t)segment_address + segment_length);
        }

        inline int get_page_index(void* _p) {
            auto it = page_index.find(_p);
            return (it == page_index.end()) ? -1 : it->second;
        }

        static inline void* UMAP_PAGE_BEGIN(const void* a) {
            return (void*)((uint64_t)a & ~(page_size-1));
        }

    private:
        void* segment_address;
        size_t segment_length;
        int backingfile_fd;
        vector<umap_backing_file> bk_files;
        int page_buffer_size;
        bool time_to_stop;
        uint64_t fault_count;
        int userfault_fd;
        int next_page_alloc_index;
        thread *listener;
        vector<umap_page> pages_in_memory;
        char* tmppagebuf;

        map<void*, int> page_index;

        void evict_page(umap_page& page);
        void remove_page_index(void* _p) { page_index.erase(_p); }
        void uffd_handler(void);
        void pagefault_event(const struct uffd_msg& msg);
        inline void stop_faultlistener( void ) noexcept {
            time_to_stop = true;
            listener->join();
        }

        inline void add_page_index(int idx, void* page) {
            page_index[page] = idx;
        }

        void delete_page_index(void* page) {
            int num_erased;
            num_erased = page_index.erase(page);
            assert(num_erased == 1);
        }

        void enable_wp_on_pages_and_wake(uint64_t, int64_t);
        void disable_wp_on_pages(uint64_t, int64_t);
};

class umap_page {
    public:
        umap_page(): page{nullptr}, dirty{false} {};
        bool page_is_dirty() { return dirty; }
        void mark_page_dirty() { dirty = true; }
        void mark_page_clean() { dirty = false; }
        void* get_page(void) { return page; }
        void set_page(void* _p) { page = _p; }
    private:
        void* page;
        bool dirty;
};

static map<void*, _umap*> active_umaps;

void* umap(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    struct stat file;
    fstat(fd,&file);
    struct umap_backing_file file1={.fd = fd, .data_size = file.st_size, .data_offset = offset};
    return umap_mf(addr, length, prot, flags, 1, &file1);
}

//--------------------------for multi-file support----------------------
void* umap_mf(void* addr, size_t length, int prot, int flags, int num_backing_file, umap_backing_file* backing_files)
{
    if (!(flags & UMAP_PRIVATE) || flags & ~(UMAP_PRIVATE|UMAP_FIXED)) {
        cerr << "umap: Invalid flags: " << hex << flags << endl;
        return UMAP_FAILED;
    }

    flags |= (MAP_ANONYMOUS | MAP_NORESERVE);

    void* region = mmap(addr, length, prot, flags, -1, 0);
 
    if (region == MAP_FAILED) {
        perror("ERROR: mmap failed: ");
        return UMAP_FAILED;
    }

    _umap *p_umap;
    try {
        p_umap = new _umap{region, length, num_backing_file, backing_files};
    } catch(const std::exception& e) {
        cerr << __FUNCTION__ << " Failed to launch _umap: " << e.what() << endl;
        return UMAP_FAILED;
    } catch(...) {
        cerr << "umap failed to instantiate _umap object\n";
        return UMAP_FAILED;
    }

    active_umaps[region] = p_umap;
    return region;
}

int uunmap(void*  addr, size_t length)
{
    auto it = active_umaps.find(addr);

    if (it != active_umaps.end()) {
        it->second->uffd_finalize();
        delete it->second;
        active_umaps.erase(it);
    }
    return 0;
}

int umap_cfg_get_bufsize( void )
{
    return umap_page_bufsize;
}

void umap_cfg_set_bufsize( int page_bufsize )
{
    umap_page_bufsize = page_bufsize;
}

//--------------------------for multi-file support----------------------
_umap::_umap(void* _mmap_addr, size_t _mmap_length,int num_backing_file,umap_backing_file* backing_files)
    :   segment_address{_mmap_addr}, segment_length{_mmap_length},
    time_to_stop{false}, fault_count{0}, next_page_alloc_index{0}
{
    for (int i=0;i<num_backing_file;i++)
        bk_files.push_back(backing_files[i]); 
    page_buffer_size = umap_page_bufsize;

    if ((userfault_fd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK)) < 0) {
        perror("ERROR: userfaultfd syscall not available in this kernel");
        throw -1;
    }

    struct uffdio_api uffdio_api = { .api = UFFD_API, .features = 0};

    if (ioctl(userfault_fd, UFFDIO_API, &uffdio_api) == -1) {
        perror("ERROR: ioctl(UFFDIO_API)");
        throw -1;
    }

    if (uffdio_api.api != UFFD_API) {
        cerr << __FUNCTION__ << ": unsupported userfaultfd api\n";
        throw -1;
    }

    struct uffdio_register uffdio_register = {
        .range = {.start = (uint64_t)segment_address, .len = segment_length},
        .mode = UFFDIO_REGISTER_MODE_MISSING | UFFDIO_REGISTER_MODE_WP
    };

    if (ioctl(userfault_fd, UFFDIO_REGISTER, &uffdio_register) == -1) {
        perror("ERROR: ioctl/uffdio_register");
        close(userfault_fd);
        throw -1;
    }

    enable_wp_on_pages_and_wake((uint64_t)segment_address, segment_length / page_size);

    if ((uffdio_register.ioctls & UFFD_API_RANGE_IOCTLS) != UFFD_API_RANGE_IOCTLS) {
        cerr << "unexpected userfaultfd ioctl set\n";
        close(userfault_fd);
        throw -1;
    }

    if (posix_memalign((void**)&tmppagebuf, (size_t)512, page_size)) {
        perror("ERROR: posix_memalign:");
        throw -1;
    }

    if (tmppagebuf == nullptr) {
        cerr << "Unable to allocate 512 bytes for temporary buffer\n";
        close(userfault_fd);
        throw -1;
    }

    umap_page ump;
    pages_in_memory.resize(page_buffer_size, ump);

    backingfile_fd=bk_files[0].fd;
    listener = new thread{&_umap::uffd_handler,this};
}

void _umap::uffd_handler(void)
{
    //cout << __FUNCTION__ << " on CPU " << sched_getcpu() << " Started\n";
    for (;;) {
        struct uffd_msg msg;

        struct pollfd pollfd[1];
        pollfd[0].fd = userfault_fd;
        pollfd[0].events = POLLIN;

        // wait for a userfaultfd event to occur
        int pollres = poll(pollfd, 1, 2000);

        if (time_to_stop)
            return;

        switch (pollres) {
        case -1:
            perror("ERROR: poll/userfaultfd");
            continue;
        case 0:
            continue;
        case 1:
            break;
        default:
            cerr << __FUNCTION__ << " unexpected uffdio poll result\n";
            exit(1);
        }

        if (pollfd[0].revents & POLLERR) {
            cerr << __FUNCTION__ << " POLLERR\n";
            exit(1);
        }

        if (!pollfd[0].revents & POLLIN)
            continue;

        int readres = read(userfault_fd, &msg, sizeof(msg));
        if (readres == -1) {
            if (errno == EAGAIN)
                continue;
            perror("ERROR: read/userfaultfd");
            exit(1);
        }

        if (readres != sizeof(msg)) {
            cerr << __FUNCTION__ << "invalid msg size\n";
            exit(1);
        }

        if (msg.event != UFFD_EVENT_PAGEFAULT) {
            cerr << __FUNCTION__ << " Unexpected event " << hex << msg.event << endl;
            continue;
        }
 
        pagefault_event(msg);       // At this point, we know we have had a page fault.  Let's handle it.
    }
}

void _umap::pagefault_event(const struct uffd_msg& msg)
{
    fault_count++;
    void* fault_addr = (void*)msg.arg.pagefault.address;
    void* page_begin = UMAP_PAGE_BEGIN(fault_addr);
    int bufidx = get_page_index(page_begin);

    //
    // Check to see if the faulting page is already in memory. This can
    // happen if more than one thread causes a fault for the same page.
    //
    if (bufidx >= 0) {
        if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
#ifdef DEBUG
            if ((msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) == (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE))
                umapdbg("PF(WP+WRITE) (In Memory Already) @(%p)=%lu %s\n", page_begin, *(uint64_t*)page_begin, pages_in_memory[bufidx].page_is_dirty() ? "Already Dirty" : "Clean");
            else if (msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WP)
                umapdbg("PF(WP)       (In Memory Already) @(%p)=%lu %s\n", page_begin, *(uint64_t*)page_begin, pages_in_memory[bufidx].page_is_dirty() ? "Already Dirty" : "Clean");
            else if (msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WRITE)
                umapdbg("PF(WRITE)    (In Memory Already) @(%p)=%lu %s\n", page_begin, *(uint64_t*)page_begin, pages_in_memory[bufidx].page_is_dirty() ? "Already Dirty" : "Clean");
#endif // DEBUG

            pages_in_memory[bufidx].mark_page_dirty();
            //memcpy(tmppagebuf, page_begin, page_size);

            //struct uffdio_copy copy;
            //copy.src = (uint64_t)tmppagebuf;
            //copy.dst = (uint64_t)page_begin;
            //copy.len = page_size;
            //copy.mode = UFFDIO_COPY_MODE_DONTWAKE;
            //if (ioctl(userfault_fd, UFFDIO_COPY, &copy) == -1) {
                //perror("ERROR: ioctl(UFFDIO_COPY3 nowake)");
                //exit(1);
            //}

            disable_wp_on_pages((uint64_t)page_begin, 1);
        }
#ifdef DEBUG
        else {
            umapdbg("PF(READ)     (In Memory Already) @(%p)=%lu\n", page_begin, *(uint64_t*)page_begin);
        }
#endif // DEBUG

        struct uffdio_range wake;
        wake.start = (uint64_t)page_begin;
        wake.len = page_size; 

        if (ioctl(userfault_fd, UFFDIO_WAKE, &wake) == -1) {
            perror("ERROR: ioctl(UFFDIO_WAKE)");
            exit(1);
        }
        return;
    }

    //
    // Page not in memory, read it in and (potentially) evict someone
    //
    //
    //-----------------------for multi-file support--------------------
    int file_id=0;
    off_t offset=(uint64_t)page_begin - (uint64_t)segment_address;
    //find the file id and offset number
    file_id=offset/bk_files[0].data_size;
    offset%=bk_files[0].data_size;

    if (pread(bk_files[file_id].fd, tmppagebuf, page_size, offset+bk_files[file_id].data_offset) == -1) {
        perror("ERROR: pread failed");
        exit(1);
    }

    if (pages_in_memory[next_page_alloc_index].get_page()) {
        delete_page_index(pages_in_memory[next_page_alloc_index].get_page());
        evict_page(pages_in_memory[next_page_alloc_index]);
    }
    pages_in_memory[next_page_alloc_index].set_page(page_begin);
    add_page_index(next_page_alloc_index, page_begin);

    struct uffdio_copy copy;
    copy.src = (uint64_t)tmppagebuf;
    copy.dst = (uint64_t)page_begin;
    copy.len = page_size;

    if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
        pages_in_memory[next_page_alloc_index].mark_page_dirty();

        copy.mode = UFFDIO_COPY_MODE_DONTWAKE;
        if (ioctl(userfault_fd, UFFDIO_COPY, &copy) == -1) {
            perror("ERROR: ioctl(UFFDIO_COPY nowake)");
            exit(1);
        }

#ifdef DEBUG
        if ((msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) == (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE))
            umapdbg("PF(WP+WRITE) (UFFDIO_COPY)       @(%p)=%lu\n", page_begin, *(uint64_t*)page_begin);
        else if (msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WP)
            umapdbg("PF(WP)       (UFFDIO_COPY)       @(%p)=%lu\n", page_begin, *(uint64_t*)page_begin);
        else if (msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WRITE)
            umapdbg("PF(WRITE)    (UFFDIO_COPY)       @(%p)=%lu\n", page_begin, *(uint64_t*)page_begin);
#endif // DEBUG

        disable_wp_on_pages((uint64_t)page_begin, 1);

        struct uffdio_range wake;
        wake.start = (uint64_t)page_begin;
        wake.len = page_size;

        if (ioctl(userfault_fd, UFFDIO_WAKE, &wake) == -1) {
            perror("ERROR: ioctl(UFFDIO_WAKE)");
            exit(1);
        }
    }
    else {
        pages_in_memory[next_page_alloc_index].mark_page_clean();

        copy.mode = UFFDIO_COPY_MODE_DONTWAKE;
        if (ioctl(userfault_fd, UFFDIO_COPY, &copy) == -1) {
            perror("ERROR: ioctl(UFFDIO_COPY nowake)");
            exit(1);
        }

        umapdbg("PF(READ)     (UFFDIO_COPY)       @(%p)=%lu\n", page_begin, *(uint64_t*)page_begin);

        enable_wp_on_pages_and_wake((uint64_t)page_begin, 1);

        //
        // There is a very small window between UFFDIO_COPY_MODE and enable_wp_on_pages_and_wake where
        // a write may occur before we re-enable WP (the UFFDIO_COPY appears to clear any previously 
        // set WP settings.
        //
        if (memcmp(tmppagebuf, page_begin, page_size)) {
            pages_in_memory[next_page_alloc_index].mark_page_dirty();
            disable_wp_on_pages((uint64_t)page_begin, 1);
            umapdbg("PF(READ) %p changed after UFFDIO_COPY\n", page_begin);
        }

        struct uffdio_range wake;
        wake.start = (uint64_t)page_begin;
        wake.len = page_size;

        if (ioctl(userfault_fd, UFFDIO_WAKE, &wake) == -1) {
            perror("ERROR: ioctl(UFFDIO_WAKE)");
            exit(1);
        }
    }
    next_page_alloc_index = (next_page_alloc_index +1) % page_buffer_size;
}

void _umap::evict_page(umap_page& pb)
{
    uint64_t* page = (uint64_t*)pb.get_page();

    if (pb.page_is_dirty()) {
        // Prevent further writes.  No need to do this if not dirty because WP is already on.
        //
        // Preventing further writes is problematic because the kernel module will wake up any threads that might be waiting for a fault
        // to be handled in this page.
        //
        // It is possible to work around this by making sure that all faults and WP exceptions for this page have been handled prior to
        // evicting the page.
        //
        enable_wp_on_pages_and_wake((uint64_t)page, 1);

        umapdbg("EVICT(DIRTY)  @(%p)=%lu\n", page, *page);

        if (pwrite(backingfile_fd, (void*)page, page_size, (off_t)((uint64_t)page - (uint64_t)segment_address)) == -1) {
            perror("ERROR: pwrite failed");
            assert(0);
        }
    }
#ifdef DEBUG
    else {
        umapdbg("EVICT(CLEAN)  @(%p)=%lu\n", page, *page);
    }
#endif // DEBUG

    if (madvise((void*)page, page_size, MADV_DONTNEED) == -1) {
        perror("ERROR: madvise");
        assert(0);
    } 

    pb.set_page(nullptr);
}

//
// Enabling WP always wakes up any sleeping thread that may have been faulted in the specified range.
//
// For reasons which are unknown, the kernel module interface for UFFDIO_WRITEPROTECT does not allow for the caller to submit
// UFFDIO_WRITEPROTECT_MODE_DONTWAKE when enabling WP with UFFDIO_WRITEPROTECT_MODE_WP.  UFFDIO_WRITEPROTECT_MODE_DONTWAKE is only 
// allowed when disabling WP.
//
void _umap::enable_wp_on_pages_and_wake(uint64_t start, int64_t num_pages)
{
    struct uffdio_writeprotect wp;
    wp.range.start = start;
    wp.range.len = num_pages * page_size;
    wp.mode = UFFDIO_WRITEPROTECT_MODE_WP;

    umapdbg("+WRITEPROTECT  (%p -- %p)\n", (void*)start, (void*)(start+(num_pages*page_size))); 

    if (ioctl(userfault_fd, UFFDIO_WRITEPROTECT, &wp) == -1) {
        perror("ERROR: ioctl(UFFDIO_WRITEPROTECT Enable)");
        exit(1);
    }
}

//
// We intentionally do not wake up faulting thread when disabling WP.  This is to handle the write-fault case when the page needs to be copied in.
//
void _umap::disable_wp_on_pages(uint64_t start, int64_t num_pages)
{
    struct uffdio_writeprotect wp;
    wp.range.start = start;
    wp.range.len = page_size * num_pages;
    wp.mode = UFFDIO_WRITEPROTECT_MODE_DONTWAKE;

    umapdbg("-WRITEPROTECT  (%p -- %p)\n", (void*)start, (void*)(start+(num_pages*page_size))); 

    if (ioctl(userfault_fd, UFFDIO_WRITEPROTECT, &wp) == -1) {
        perror("ERROR: ioctl(UFFDIO_WRITEPROTECT Disable)");
        exit(1);
    }
}

void _umap::uffd_finalize()
{
    for (auto it : pages_in_memory) {
        if (it.get_page()) {
            delete_page_index(it.get_page());
            evict_page(it);
        }
    }

    stop_faultlistener();

    struct uffdio_register uffdio_register;
    uffdio_register.range.start = (uint64_t)segment_address;
    uffdio_register.range.len = segment_length;

    if (ioctl(userfault_fd, UFFDIO_UNREGISTER, &uffdio_register.range)) {
        perror("ERROR: UFFDIO_UNREGISTER");
        exit(1);
    }
}

static struct sigaction saved_sa;
static uint64_t num_bus_errs = 0;

void sighandler(int signum, siginfo_t *info, void* buf)
{
    if (signum != SIGBUS) {
        cerr << "Unexpected signal: " << signum << " received\n";
        exit(1);
    }

    void* page_begin = _umap::UMAP_PAGE_BEGIN(info->si_addr);
 
    for (auto it : active_umaps) {
        if (it.second->is_in_umap(page_begin)) {
            num_bus_errs++;
#ifdef DEBUG
            if (it.second->get_page_index(page_begin) >= 0)
                umapdbg("SIGBUS %p (page=%p) ALREADY IN UMAP PAGE BUFFER!\n", info->si_addr, page_begin); 
            else
                umapdbg("SIGBUS %p (page=%p) Not currently in umap page buffer\n", info->si_addr, page_begin); 
#endif // DEBUG
            return;
        }
    }
    umapdbg("SIGBUS %p (page=%p) ADDRESS OUTSIDE OF UMAP RANGE\n", info->si_addr, page_begin); 
}

void __attribute ((constructor)) init_umap_lib( void )
{
    struct sigaction act;

    umaplog_init();

    if ((page_size = sysconf(_SC_PAGESIZE)) == -1) {
        perror("ERROR: sysconf(_SC_PAGESIZE)");
        throw -1;
    }

    act.sa_handler = NULL;
    act.sa_sigaction = sighandler;
    if (sigemptyset(&act.sa_mask) == -1) {
        perror("ERROR: sigemptyset: ");
        exit(1);
    }

    act.sa_flags = SA_NODEFER | SA_SIGINFO;

    if (sigaction(SIGBUS, &act, &saved_sa) == -1) {
        perror("ERROR: sigaction: ");
        exit(1);
    }
}

void __attribute ((destructor)) fine_umap_lib( void )
{
    if (sigaction(SIGBUS, &saved_sa, NULL) == -1) {
        perror("ERROR: sigaction restore: ");
        exit(1);
    }

    for (auto it : active_umaps) {
        it.second->uffd_finalize();
        delete it.second;
    }
}
