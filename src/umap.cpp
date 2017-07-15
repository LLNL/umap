/*
 * This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at
 * https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) 
 * version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
 * and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the 
 * GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
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
#include "umap.h"               // API to library

using namespace std;

const int UMAP_VERSION_MAJOR = 0;
const int UMAP_VERSION_MINOR = 0;
const int UMAP_VERSION_PATCH = 1;
const int UMAP_DEFAULT_PBSIZE = 16;
static int umap_page_bufsize = UMAP_DEFAULT_PBSIZE;

class umap_page;
class _umap {
    public:
        _umap(void* _mmap_addr, size_t _mmap_length, int _mmap_fd);
        _umap(void* _mmap_addr, size_t _mmap_length, int _mmap_fd, int* fd_list, off_t data_offset ,off_t frame);

        inline void stop_faultlistener( void ) noexcept {
            time_to_stop = true;
            listener->join();
        }

        inline int get_page_index(void* _p) {
            auto it = page_index.find(_p);
            return (it == page_index.end()) ? -1 : it->second;
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
        void* uffd_handler(void);
        void* uffd_fits_handler(void);
        int uffd_finalize(void);
              
    private:
        void* segment_address;
        size_t segment_length;
        int backingfile_fd;
        int page_buffer_size;
        bool time_to_stop;
        uint64_t fault_count;
        int userfault_fd;
        int next_page_alloc_index;
        long page_size;
        thread *listener;
        vector<umap_page> pages_in_memory;
        char* tmppagebuf;
  //--------for multi-file fits support---------
        int number_file;
        int* fd_list;
        off_t fits_offset;
        off_t frame_size;

        map<void*, int> page_index;

        void evict_page(umap_page& page);
        void remove_page_index(void* _p) { page_index.erase(_p); }
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
    if (!(flags & UMAP_PRIVATE) || flags & ~(UMAP_PRIVATE|UMAP_FIXED)) {
        cerr << "umap: Invalid flags: " << hex << flags << endl;
        return UMAP_FAILED;
    }

    flags |= (MAP_ANONYMOUS | MAP_NORESERVE);

    void* region = mmap(addr, length, prot, flags, -1, offset);
    if (region == MAP_FAILED) {
        perror("mmap failed: ");
        return UMAP_FAILED;
    }

    try {
        _umap *p_umap = new _umap{region, length, fd};
        active_umaps[region] = p_umap;
    }
    catch(...) {
        return UMAP_FAILED;
    }

    return region;
}
//--------------------------for multi-file support----------------------
void* umap_fits(void* addr, size_t length, int prot, int flags, int fd_num,int* fd_list,off_t offset, off_t frame)
{
    if (!(flags & UMAP_PRIVATE) || flags & ~(UMAP_PRIVATE|UMAP_FIXED)) {
        cerr << "umap: Invalid flags: " << hex << flags << endl;
        return UMAP_FAILED;
    }

    flags |= (MAP_ANONYMOUS | MAP_NORESERVE);

    void* region = mmap(addr, length, prot, flags, -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap failed: ");
        return UMAP_FAILED;
    }

    try {
      _umap *p_umap = new _umap{region, length, fd_num,fd_list,offset,frame};
        active_umaps[region] = p_umap;
    }
    catch(...) {
        return UMAP_FAILED;
    }

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

_umap::_umap(  void* _mmap_addr, size_t _mmap_length, int _mmap_fd)
    :   segment_address{_mmap_addr}, segment_length{_mmap_length},
        backingfile_fd{_mmap_fd}, 
        time_to_stop{false}, fault_count{0}, next_page_alloc_index{0}
{
    page_buffer_size = umap_page_bufsize;
    if ((page_size = sysconf(_SC_PAGESIZE)) == -1) {
        perror("sysconf(_SC_PAGESIZE)");
        throw -1;
    }

    if ((userfault_fd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK)) < 0) {
        perror("userfaultfd syscall not available in this kernel");
        throw -1;
    }

    struct uffdio_api uffdio_api = {        // enable for api version and check features
        .api = UFFD_API,
        .features = 0
    };

    if (ioctl(userfault_fd, UFFDIO_API, &uffdio_api) == -1) {
        perror("ioctl(UFFDIO_API)");
        throw -1;
    }

    if (uffdio_api.api != UFFD_API) {
        cerr << __FUNCTION__ << ": unsupported userfaultfd api\n";
        throw -1;
    }

    struct uffdio_register uffdio_register = {
        .range = {
            .start = (uint64_t)segment_address,
            .len = segment_length
        },
        .mode = UFFDIO_REGISTER_MODE_MISSING | UFFDIO_REGISTER_MODE_WP
    };

    if (ioctl(userfault_fd, UFFDIO_REGISTER, &uffdio_register) == -1) {
        perror("ioctl/uffdio_register");
        close(userfault_fd);
        throw -1;
    }

    enable_wp_on_pages_and_wake((uint64_t)segment_address, segment_length / page_size);

    if ((uffdio_register.ioctls & UFFD_API_RANGE_IOCTLS) != UFFD_API_RANGE_IOCTLS) {
        cerr << "unexpected userfaultfd ioctl set\n";
        close(userfault_fd);
        throw -1;
    }

    posix_memalign((void**)&tmppagebuf, (size_t)512, page_size);
    if (tmppagebuf == nullptr) {
        cerr << "Unable to allocate 512 bytes for temporary buffer\n";
        close(userfault_fd);
        throw -1;
    }

    umap_page ump;
    pages_in_memory.resize(page_buffer_size, ump);

    listener = new thread{&_umap::uffd_handler, this};      // Start our userfaultfd listener
}

//--------------------------for multi-file support----------------------
_umap::_umap(void* _mmap_addr, size_t _mmap_length, int _mmap_fd,int* file_list, off_t data_offset, off_t frame)
    :   segment_address{_mmap_addr}, segment_length{_mmap_length},
  backingfile_fd{-1},number_file{_mmap_fd},fits_offset{data_offset},fd_list{file_list},frame_size{frame},
        time_to_stop{false}, fault_count{0}, next_page_alloc_index{0}
{
    page_buffer_size = umap_page_bufsize;
    if ((page_size = sysconf(_SC_PAGESIZE)) == -1) {
        perror("sysconf(_SC_PAGESIZE)");
        throw -1;
    }

    if ((userfault_fd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK)) < 0) {
        perror("userfaultfd syscall not available in this kernel");
        throw -1;
    }

    struct uffdio_api uffdio_api = {        // enable for api version and check features
        .api = UFFD_API,
        .features = 0
    };

    if (ioctl(userfault_fd, UFFDIO_API, &uffdio_api) == -1) {
        perror("ioctl(UFFDIO_API)");
        throw -1;
    }

    if (uffdio_api.api != UFFD_API) {
        cerr << __FUNCTION__ << ": unsupported userfaultfd api\n";
        throw -1;
    }

    struct uffdio_register uffdio_register = {
        .range = {
            .start = (uint64_t)segment_address,
            .len = segment_length
        },
        .mode = UFFDIO_REGISTER_MODE_MISSING | UFFDIO_REGISTER_MODE_WP
    };

    if (ioctl(userfault_fd, UFFDIO_REGISTER, &uffdio_register) == -1) {
        perror("ioctl/uffdio_register");
        close(userfault_fd);
        throw -1;
    }

    enable_wp_on_pages_and_wake((uint64_t)segment_address, segment_length / page_size);

    if ((uffdio_register.ioctls & UFFD_API_RANGE_IOCTLS) != UFFD_API_RANGE_IOCTLS) {
        cerr << "unexpected userfaultfd ioctl set\n";
        close(userfault_fd);
        throw -1;
    }

    posix_memalign((void**)&tmppagebuf, (size_t)512, page_size);
    if (tmppagebuf == nullptr) {
        cerr << "Unable to allocate 512 bytes for temporary buffer\n";
        close(userfault_fd);
        throw -1;
    }

    umap_page ump;
    pages_in_memory.resize(page_buffer_size, ump);

    listener = new thread{&_umap::uffd_fits_handler, this};      // Start our userfaultfd listener
}

void* _umap::uffd_handler(void)
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
            perror("read/userfaultfd");
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
 
        //
        // At this point, we know we have had a page fault.  Let's handle it.
        //
#define PAGE_BEGIN(a)   (void*)((uint64_t)a & ~(page_size-1));

        fault_count++;
        void* fault_addr = (void*)msg.arg.pagefault.address;
        void* page_begin = PAGE_BEGIN(fault_addr);

        //
        // Check to see if the faulting page is already in memory. This can
        // happen if more than one thread causes a fault for the same page.
        //
        int bufidx = get_page_index(page_begin);

        if (bufidx >= 0) {
            if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
                pages_in_memory[bufidx].mark_page_dirty();
                disable_wp_on_pages((uint64_t)page_begin, 1);
            }

            struct uffdio_range wake;
            wake.start = (uint64_t)page_begin;
            wake.len = page_size; 

            if (ioctl(userfault_fd, UFFDIO_WAKE, &wake) == -1) {
                perror("ioctl(UFFDIO_WAKE)");
                exit(1);
            }
            continue;
        }

        //
        // Page not in memory, read it in and evict someone
        //
        ssize_t pread_ret = pread(backingfile_fd, tmppagebuf, page_size,
                       (off_t)((uint64_t)page_begin - (uint64_t)segment_address));

        if (pread_ret == -1) {
            perror("pread failed");
            exit(1);
        }

        if (pages_in_memory[next_page_alloc_index].get_page()) {
            delete_page_index(pages_in_memory[next_page_alloc_index].get_page());
            evict_page(pages_in_memory[next_page_alloc_index]);
        }

        pages_in_memory[next_page_alloc_index].set_page(page_begin);
        add_page_index(next_page_alloc_index, page_begin);

        if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
            disable_wp_on_pages((uint64_t)page_begin, 1);
            pages_in_memory[next_page_alloc_index].mark_page_dirty();
        }
        else {
            pages_in_memory[next_page_alloc_index].mark_page_clean();
        }

        next_page_alloc_index = (next_page_alloc_index +1) % page_buffer_size;

        struct uffdio_copy copy;
        copy.src = (uint64_t)tmppagebuf;
        copy.dst = (uint64_t)page_begin;
        copy.len = page_size;

        if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
            copy.mode = 0;
            if (ioctl(userfault_fd, UFFDIO_COPY, &copy) == -1) {
                perror("ioctl(UFFDIO_COPY wake)");
                exit(1);
            }
        }
        else {
            copy.mode = UFFDIO_COPY_MODE_DONTWAKE;
            if (ioctl(userfault_fd, UFFDIO_COPY, &copy) == -1) {
                perror("ioctl(UFFDIO_COPY nowake)");
                exit(1);
            }
            enable_wp_on_pages_and_wake((uint64_t)page_begin, 1);
        }
    }
    return NULL;
}
//-----------------------for multi-file support--------------------
void* _umap::uffd_fits_handler(void)
{
    //struct stat fileinfo;
    //fstat(fd_list[0],&fileinfo);
    //cout << __FUNCTION__ << " on CPU " << sched_getcpu() << " Started\n";
    for (;;) {
        struct uffd_msg msg;

        struct pollfd pollfd[1];
        pollfd[0].fd = userfault_fd;
        pollfd[0].events = POLLIN;

        // wait for a userfaultfd event to occur
        int pollres = poll(pollfd, 1, 2000);

        if (time_to_stop)
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
            perror("read/userfaultfd");
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
 
        //
        // At this point, we know we have had a page fault.  Let's handle it.
        //
#define PAGE_BEGIN(a)   (void*)((uint64_t)a & ~(page_size-1));

        fault_count++;
        void* fault_addr = (void*)msg.arg.pagefault.address;
        void* page_begin = PAGE_BEGIN(fault_addr);

        //
        // Check to see if the faulting page is already in memory. This can
        // happen if more than one thread causes a fault for the same page.
        //
        int bufidx = get_page_index(page_begin);

        if (bufidx >= 0) {
            if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
                pages_in_memory[bufidx].mark_page_dirty();
                disable_wp_on_pages((uint64_t)page_begin, 1);
            }

            struct uffdio_range wake;
            wake.start = (uint64_t)page_begin;
            wake.len = page_size; 

            if (ioctl(userfault_fd, UFFDIO_WAKE, &wake) == -1) {
                perror("ioctl(UFFDIO_WAKE)");
                exit(1);
            }
            continue;
        }

        //
        // Page not in memory, read it in and evict someone
        //
	int file_id=0;
	off_t offset=(uint64_t)page_begin - (uint64_t)segment_address;
        //find the file id and offset number                                              
        while (offset>=frame_size)
	{
            file_id++;
            offset-=frame_size;
	}

        ssize_t pread_ret = pread(fd_list[file_id], tmppagebuf, page_size,
                       offset+fits_offset);

        if (pread_ret == -1) {
            perror("pread failed");
            exit(1);
        }

        if (pages_in_memory[next_page_alloc_index].get_page()) {
            delete_page_index(pages_in_memory[next_page_alloc_index].get_page());
            evict_page(pages_in_memory[next_page_alloc_index]);
        }

        pages_in_memory[next_page_alloc_index].set_page(page_begin);
        add_page_index(next_page_alloc_index, page_begin);

        if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
            disable_wp_on_pages((uint64_t)page_begin, 1);
            pages_in_memory[next_page_alloc_index].mark_page_dirty();
        }
        else {
            pages_in_memory[next_page_alloc_index].mark_page_clean();
        }

        next_page_alloc_index = (next_page_alloc_index +1) % page_buffer_size;

        struct uffdio_copy copy;
        copy.src = (uint64_t)tmppagebuf;
        copy.dst = (uint64_t)page_begin;
        copy.len = page_size;

        if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
            copy.mode = 0;
            if (ioctl(userfault_fd, UFFDIO_COPY, &copy) == -1) {
                perror("ioctl(UFFDIO_COPY wake)");
                exit(1);
            }
        }
        else {
            copy.mode = UFFDIO_COPY_MODE_DONTWAKE;
            if (ioctl(userfault_fd, UFFDIO_COPY, &copy) == -1) {
                perror("ioctl(UFFDIO_COPY nowake)");
                exit(1);
            }
            enable_wp_on_pages_and_wake((uint64_t)page_begin, 1);
        }
    }
    return NULL;
}


void _umap::evict_page(umap_page& pb)
{
    if (pb.page_is_dirty()&&(backingfile_fd!=-1)) { //--------if multi-file, no pwrite needed
        // Prevent further writes.  No need to do this if not dirty because
        // WP is already on.
        //
        // Preventing further writes is problematic because the kernel
        // module will wake up any threads that might be waiting for a fault
        // to be handled in this page.
        //
        // It is possible to work around this by making sure that all faults
        // and WP exceptions for this page have been handled prior to evicting
        // the page.
        //
        enable_wp_on_pages_and_wake((uint64_t)pb.get_page(), 1);

        ssize_t rval;

        rval = pwrite(backingfile_fd, (void*)(pb.get_page()), page_size, 
                        (off_t)((uint64_t)pb.get_page() - (uint64_t)segment_address));

        if (rval == -1) {
            perror("pwrite failed");
            assert(0);
        }
    }

    if (madvise((void*)(pb.get_page()), page_size, MADV_DONTNEED) == -1) {
        perror("madvise");
        assert(0);
    } 

    pb.set_page(nullptr);
}

//
// Enabling WP always wakes up any sleeping thread that may have been faulted
// in the specified range.
//
// For reasons I don't understand, the kernel module interface for 
// UFFDIO_WRITEPROTECT does not allow for the caller to submit
// UFFDIO_WRITEPROTECT_MODE_DONTWAKE when enabling WP with
// UFFDIO_WRITEPROTECT_MODE_WP.  UFFDIO_WRITEPROTECT_MODE_DONTWAKE is only 
// allowed when disabling WP.
//
void _umap::enable_wp_on_pages_and_wake(uint64_t start, int64_t num_pages)
{
    struct uffdio_writeprotect wp;
    wp.range.start = start;
    wp.range.len = num_pages * page_size;
    wp.mode = UFFDIO_WRITEPROTECT_MODE_WP;

    if (ioctl(userfault_fd, UFFDIO_WRITEPROTECT, &wp) == -1) {
        perror("ioctl(UFFDIO_WRITEPROTECT Enable)");
        exit(1);
    }
}

//
// We intentionally do not wake up faulting thread when disabling WP.  This
// is to handle the write-fault case when the page needs to be copied in.
//
void _umap::disable_wp_on_pages(uint64_t start, int64_t num_pages)
{
    struct uffdio_writeprotect wp;
    wp.range.start = start;
    wp.range.len = page_size * num_pages;
    wp.mode = UFFDIO_WRITEPROTECT_MODE_DONTWAKE;

    if (ioctl(userfault_fd, UFFDIO_WRITEPROTECT, &wp) == -1) {
        perror("ioctl(UFFDIO_WRITEPROTECT Disable)");
        exit(1);
    }
}

int _umap::uffd_finalize()
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
        perror("UFFDIO_UNREGISTER");
        return 1;
    }
    return 0;
}

void __attribute ((constructor)) init_umap_lib( void )
{
}

void __attribute ((destructor)) fine_umap_lib( void )
{
    for (auto it : active_umaps) {
        it.second->uffd_finalize();
        delete it.second;
    }
}
