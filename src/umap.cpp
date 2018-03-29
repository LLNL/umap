/* This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <iostream>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <thread>
#include <unordered_map>
#include <sstream>
#include <deque>
#include <string>
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
#include <sys/prctl.h>
#include "umap.h"               // API to library
#include "umaplog.h"            // umap_log()
#include "config.h"

using namespace std;

const int umap_Version_Major = UMAP_VERSION_MAJOR;
const int umap_Version_Minor = UMAP_VERSION_MINOR;
const int umap_Version_Patch = UMAP_VERSION_PATCH;

static const int UMAP_UFFD_MAX_MESSAGES = 256;
static unsigned int uffd_threads;
const uint64_t UMAP_DEFAULT_PAGES_PER_UFFD_HANDLER = 1024;  // Separate Page Buffer per Thread

static const uint64_t UMAP_PAGES_PER_BLOCK = 1024;
static uint64_t  umap_pages_per_uffd_handler = UMAP_DEFAULT_PAGES_PER_UFFD_HANDLER;

static long page_size;

class umap_page;
struct umap_PageBlock;
class umap_page_buffer;
class umap_stats;
class __umap;
class UserFaultHandler;

// 
// |------------------------- umap() provided Region ----------------------------|
// |------------------------- umap() provided backing file(s) -------------------|
// |- Page Block 1 -|- Page Block 2 -|- ... -|- Page Block N-1 -|- Page Block N -|
//
// _umap organizes a region of memory into a set of blocks of pages.  The blocks 
// of pages are then distributed evenly to a set of UserFaultHandler objects.
//
class _umap {
  friend UserFaultHandler;
  public:
    _umap(void* _region, uint64_t _rsize, int num_backing_file, umap_backing_file* backing_files);
    ~_umap();

    static inline void* UMAP_PAGE_BEGIN(const void* a) {
      return (void*)((uint64_t)a & ~(page_size-1));
    }
    vector<UserFaultHandler*> ufault_handlers;

  private:
    void* region;
    uint64_t region_size;
    int backingfile_fd;
    vector<umap_backing_file> bk_files;
    bool uffd_time_to_stop_working;
};

class UserFaultHandler {
  friend _umap;
  public:
    UserFaultHandler(_umap* _um, const vector<umap_PageBlock>& _pblks, uint64_t _pbuf_size);
    ~UserFaultHandler(void);
    void stop_uffd_worker( void ) noexcept {
      _u->uffd_time_to_stop_working = true;
      uffd_worker->join();
    };
    bool page_is_in_umap(const void* page_begin);
    umap_page_buffer* get_pagebuffer() { return pagebuffer; }
    
    umap_stats* stat;
  private:
    _umap* _u;
    vector<umap_PageBlock> PageBlocks;
    uint64_t pbuf_size;
    umap_page_buffer* pagebuffer;
    vector<struct uffd_msg> umessages;

    int userfault_fd;
    char* tmppagebuf;
    thread* uffd_worker;

    void evict_page(umap_page* page);
    void uffd_handler(void);
    void pagefault_event(const struct uffd_msg& msg);
    void enable_wp_on_pages_and_wake(uint64_t, int64_t);
    void disable_wp_on_pages(uint64_t, int64_t, bool);
};

class umap_stats {
  public:
    umap_stats(): 
      stat_faults{0}, 
      dirty_evicts{0}, 
      clean_evicts{0}, 
      wp_messages{0}, 
      read_faults{0}, 
      write_faults{0}, 
      sigbus{0}, 
      stuck_wp{0},
      dropped_dups{0}
      {};

    void print_stats(void);

    uint64_t stat_faults;
    uint64_t dirty_evicts;
    uint64_t clean_evicts;
    uint64_t wp_messages;
    uint64_t read_faults;
    uint64_t write_faults;
    uint64_t sigbus;
    uint64_t stuck_wp;
    uint64_t dropped_dups;
};

struct umap_PageBlock {
    void*  base;
    uint64_t length;
};

class umap_page_buffer {
  /*
   * TODO: Make the single page buffer threadsafe
   */
  public:
    umap_page_buffer(uint64_t pbuffersize);
    ~umap_page_buffer();
    umap_page* alloc_page_desc(void* page);
    void dealloc_page_desc(umap_page* page_desc);

    void add_page_desc_to_inmem(umap_page* page_desc);
    umap_page* get_page_desc_to_evict();
    umap_page* find_inmem_page_desc(void* page_addr); // Finds page_desc for page_addr in inmem_page_descriptors

  private:
    uint64_t page_buffer_size;
    deque<umap_page*> free_page_descriptors;
    deque<umap_page*> inmem_page_descriptors;
    unordered_map<void*, umap_page*> inmem_page_map;
};

class umap_page {
  public:
    umap_page(): page{nullptr}, dirty{false} {}
    bool page_is_dirty() { return dirty; }
    void mark_page_dirty() { dirty = true; }
    void mark_page_clean() { dirty = false; }
    void* get_page(void) { return page; }

    void set_page(void* _p);
  private:
    void* page;
    bool dirty;
};

static unordered_map<void*, _umap*> active_umaps;

//
// Library Interface Entry
//
static int check_uffd_compatibility( void )
{
  int fd;

  if ((fd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK)) < 0) {
    perror("UFFD Compatibilty Check - Unable to open userfaultfd: ");
    exit(1);
  }

  struct uffdio_api uffdio_api = { .api = UFFD_API, .features = UFFD_FEATURE_PAGEFAULT_FLAG_WP };

  if (ioctl(fd, UFFDIO_API, &uffdio_api) == -1) {
    cerr << "UFFD Compatibilty Check - userfaultfd WP Not Available\n";
    exit(1);
  }

  if (!(uffdio_api.features & UFFD_FEATURE_PAGEFAULT_FLAG_WP)) {
    cerr << "UFFD Compatibilty Check - unsupported userfaultfd WP\n";
    exit(1);
  }

  close(fd);

  return 0;
}

void* umap(void* addr, uint64_t length, int prot, int flags, int fd, off_t offset)
{
  struct stat file;

  if (check_uffd_compatibility() < 0)
    return NULL;

  fstat(fd,&file);
  struct umap_backing_file file1={.fd = fd, .data_size = file.st_size, .data_offset = offset};
  return umap_mf(addr, length, prot, flags, 1, &file1);
}

void* umap_mf(void* bass_addr, uint64_t region_size, int prot, int flags, int num_backing_file, umap_backing_file* backing_files)
{
  assert((region_size % page_size) == 0);

  if (!(flags & UMAP_PRIVATE) || flags & ~(UMAP_PRIVATE|UMAP_FIXED)) {
    cerr << "umap: Invalid flags: " << hex << flags << endl;
    return UMAP_FAILED;
  }

  void* region = mmap(bass_addr, region_size, prot, flags | (MAP_ANONYMOUS | MAP_NORESERVE), -1, 0);

  if (region == MAP_FAILED) {
    perror("ERROR: mmap failed: ");
    return UMAP_FAILED;
  }

  try {
    active_umaps[region] = new _umap{region, region_size, num_backing_file, backing_files};
  } catch(const std::exception& e) {
    cerr << __FUNCTION__ << " Failed to launch _umap: " << e.what() << endl;
    return UMAP_FAILED;
  } catch(...) {
    cerr << "umap failed to instantiate _umap object\n";
    return UMAP_FAILED;
  }
  return region;
}

int uunmap(void*  addr, uint64_t length)
{
  auto it = active_umaps.find(addr);

  if (it != active_umaps.end()) {
    delete it->second;
    active_umaps.erase(it);
  }
  return 0;
}

uint64_t umap_cfg_get_bufsize( void )
{
  return (umap_pages_per_uffd_handler * uffd_threads);
}

void umap_cfg_set_bufsize( uint64_t page_bufsize )
{
  umap_pages_per_uffd_handler = (page_bufsize / uffd_threads);

  if (umap_pages_per_uffd_handler == 0)
    umap_pages_per_uffd_handler = 1;
}

//
// Signal Handlers
//
static struct sigaction saved_sa;

void sighandler(int signum, siginfo_t *info, void* buf)
{
  if (signum != SIGBUS) {
    cerr << "Unexpected signal: " << signum << " received\n";
    exit(1);
  }

  void* page_begin = _umap::UMAP_PAGE_BEGIN(info->si_addr);

  for (auto it : active_umaps) {
    for (auto ufh : it.second->ufault_handlers) {
      if (ufh->page_is_in_umap(page_begin)) {
        ufh->stat->sigbus++;

        if (ufh->get_pagebuffer()->find_inmem_page_desc(page_begin) != nullptr)
          umapdbg("SIGBUS %p (page=%p) ALREADY IN UMAP PAGE BUFFER!\n", info->si_addr, page_begin); 
        else
          umapdbg("SIGBUS %p (page=%p) Not currently in umap page buffer\n", info->si_addr, page_begin); 
        return;
      }
    }
  }
  umapdbg("SIGBUS %p (page=%p) ADDRESS OUTSIDE OF UMAP RANGE\n", info->si_addr, page_begin); 
  assert(0);
}

void __attribute ((constructor)) init_umap_lib( void )
{
  struct sigaction act;

  umaplog_init();

  if ((page_size = sysconf(_SC_PAGESIZE)) == -1) {
    perror("ERROR: sysconf(_SC_PAGESIZE)");
    throw -1;
  }

  unsigned int n = std::thread::hardware_concurrency();
  uffd_threads = (n == 0) ? 16 : n;

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
    delete it.second;
  }
}

//
// _umap class implementation
//
_umap::_umap(void* _region, uint64_t _rsize, int num_backing_file, umap_backing_file* backing_files)
    : region{_region}, region_size{_rsize}, uffd_time_to_stop_working{false}
{
  for (int i=0;i<num_backing_file;i++)
    bk_files.push_back(backing_files[i]); 

  uint64_t pages_in_region = region_size / page_size;
  uint64_t pages_per_block = pages_in_region < UMAP_PAGES_PER_BLOCK ? pages_in_region : UMAP_PAGES_PER_BLOCK;
  uint64_t page_blocks = pages_in_region / pages_per_block;
  uint64_t remainder_of_pages_in_last_block = pages_in_region % pages_per_block;

  if (remainder_of_pages_in_last_block)
    page_blocks++;          // Account for extra block

  uint64_t num_workers = page_blocks < uffd_threads ? page_blocks : uffd_threads;
  uint64_t page_blocks_per_worker = page_blocks / num_workers;

  // cout << "umap " << num_workers << " workers for: " << region << " - " << (void*)((char*)region+region_size) << endl;

  try {
    for (uint64_t worker = 0; worker < num_workers; ++worker) {
      umap_PageBlock pb;

      pb.base = (void*)((uint64_t)region + (worker * page_blocks_per_worker * pages_per_block * page_size));
      pb.length = page_blocks_per_worker * pages_per_block * page_size;

      // If I am the last worker and we have residual pages in last block
      if ((worker == num_workers-1) && remainder_of_pages_in_last_block)
        pb.length -= ((pages_per_block - remainder_of_pages_in_last_block)) * page_size;

      vector<umap_PageBlock> segs{ pb };

      ufault_handlers.push_back( new UserFaultHandler{this, segs, umap_pages_per_uffd_handler} );
    }
  } catch(const std::exception& e) {
    cerr << __FUNCTION__ << " Failed to launch _umap: " << e.what() << endl;
    throw -1;
  } catch(...) {
    cerr << "umap failed to instantiate _umap object\n";
    throw -1;
  }
}

_umap::~_umap(void)
{
  umap_stats t;

  for ( auto handler : ufault_handlers ) {
    handler->stop_uffd_worker();
    t.stat_faults += handler->stat->stat_faults;
    t.dirty_evicts += handler->stat->dirty_evicts;
    t.clean_evicts += handler->stat->clean_evicts;
    t.wp_messages += handler->stat->wp_messages;
    t.read_faults += handler->stat->read_faults;
    t.write_faults += handler->stat->write_faults;
    t.sigbus += handler->stat->sigbus;
    t.stuck_wp += handler->stat->stuck_wp;
    t.dropped_dups += handler->stat->dropped_dups;
  }

  t.print_stats();

  for ( auto handler : ufault_handlers )
    delete handler;
}

UserFaultHandler::UserFaultHandler(_umap* _um, const vector<umap_PageBlock>& _pblks, uint64_t _pbuf_size)
    : 
      stat{ new umap_stats },
      _u{_um}, 
      PageBlocks{_pblks}, 
      pbuf_size{_pbuf_size}, 
      pagebuffer{ new umap_page_buffer{_pbuf_size} }
{
  umessages.resize(UMAP_UFFD_MAX_MESSAGES);

  if (posix_memalign((void**)&tmppagebuf, (uint64_t)512, page_size)) {
    cerr << "ERROR: posix_memalign: failed\n";
    exit(1);
  }

  if (tmppagebuf == nullptr) {
    cerr << "Unable to allocate 512 bytes for temporary buffer\n";
    exit(1);
  }

  if ((userfault_fd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK)) < 0) {
    perror("ERROR: userfaultfd syscall not available in this kernel");
    throw -1;
  }

  struct uffdio_api uffdio_api = { .api = UFFD_API, .features = UFFD_FEATURE_PAGEFAULT_FLAG_WP };

  if (ioctl(userfault_fd, UFFDIO_API, &uffdio_api) == -1) {
    perror("ERROR: UFFDIO_API Failed: ");
    exit(1);
  }

  if (!(uffdio_api.features & UFFD_FEATURE_PAGEFAULT_FLAG_WP)) {
    perror("ERROR: userfaultfd WP: ");
    exit(1);
  }

  for ( auto seg : PageBlocks ) {
    struct uffdio_register uffdio_register = {
      .range = {.start = (uint64_t)seg.base, .len = seg.length},
      .mode = UFFDIO_REGISTER_MODE_MISSING | UFFDIO_REGISTER_MODE_WP
    };

    umapdbg("Register %p\n", seg.base);

    if (ioctl(userfault_fd, UFFDIO_REGISTER, &uffdio_register) == -1) {
      perror("ERROR: ioctl/uffdio_register");
      close(userfault_fd);
      throw -1;
    }

    if ((uffdio_register.ioctls & UFFD_API_RANGE_IOCTLS) != UFFD_API_RANGE_IOCTLS) {
      cerr << "unexpected userfaultfd ioctl set\n";
      close(userfault_fd);
      throw -1;
    }
  }

  _u->backingfile_fd=_u->bk_files[0].fd;
  uffd_worker = new thread{&UserFaultHandler::uffd_handler, this};
}

UserFaultHandler::~UserFaultHandler(void)
{
  //
  // Now that all of our worker threads have stopped, we can flush everything
  //
  for ( auto seg : PageBlocks ) {
    struct uffdio_register uffdio_register;
    uffdio_register.range.start = (uint64_t)seg.base;
    uffdio_register.range.len = seg.length;

    if (ioctl(userfault_fd, UFFDIO_UNREGISTER, &uffdio_register.range)) {
      perror("ERROR: UFFDIO_UNREGISTER");
      exit(1);
    }
  }

  free(tmppagebuf);
  delete pagebuffer;
  delete stat;
  delete uffd_worker;
}

#if 0
static string uffd_pf_reason(const struct uffd_msg& msg)
{
  if ((msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) == (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE))
    return "UFFD_PAGEFAULT_FLAG_WP UFFD_PAGEFAULT_FLAG_WRITE";
  else if (msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WP)
    return "UFFD_PAGEFAULT_FLAG_WP";
  else if (msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WRITE)
    return "UFFD_PAGEFAULT_FLAG_WRITE";
  else
    return "UFFD_PAGEFAULT_READ";
}
#endif

struct less_than_key
{
  inline bool operator() (const struct uffd_msg& lhs, const struct uffd_msg& rhs)
  {
    if (lhs.arg.pagefault.address == rhs.arg.pagefault.address)
      return (lhs.arg.pagefault.flags >= rhs.arg.pagefault.address);
    else
      return (lhs.arg.pagefault.address < rhs.arg.pagefault.address);
  }
};

void UserFaultHandler::uffd_handler(void)
{
  prctl(PR_SET_NAME, "UMAP UFFD Hdlr", 0, 0, 0);
  for (;;) {
    struct pollfd pollfd[1];
    pollfd[0].fd = userfault_fd;
    pollfd[0].events = POLLIN;

    if (_u->uffd_time_to_stop_working) {
      //
      // Flush the in-memory page buffer
      // 
      for (umap_page* ep = pagebuffer->get_page_desc_to_evict(); ep != nullptr; ep = pagebuffer->get_page_desc_to_evict()) {
        evict_page(ep);
        pagebuffer->dealloc_page_desc(ep);
      }
      return;
    }

    // wait for a userfaultfd event to occur
    int pollres = poll(pollfd, 1, 2000);

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

    int readres = read(userfault_fd, &umessages[0], UMAP_UFFD_MAX_MESSAGES * sizeof(struct uffd_msg));

    if (readres == -1) {
      if (errno == EAGAIN)
        continue;
      perror("ERROR: read/userfaultfd");
      exit(1);
    }

    assert(readres % sizeof(struct uffd_msg) == 0);

    int msgs = readres / sizeof(struct uffd_msg);

    if (msgs < 1) {
      cerr << __FUNCTION__ << "invalid msg size " << readres << " " << msgs;
      exit(1);
    }

    sort(umessages.begin(), umessages.begin()+msgs, less_than_key());

#if 0
    stringstream ss;
    ss << msgs << " Messages:\n";
    for (int i = 0; i < msgs; ++i) {
      ss << "    " << uffd_pf_reason(umessages[i]) << endl;
    }
    umapdbg("%s\n", ss.str().c_str());
#endif

    uint64_t last_addr = 0;
    for (int i = 0; i < msgs; ++i) {
      if (umessages[i].event != UFFD_EVENT_PAGEFAULT) {
        cerr << __FUNCTION__ << " Unexpected event " << hex << umessages[i].event << endl;
        continue;
      }

      if (umessages[i].arg.pagefault.address == last_addr) {
        stat->dropped_dups++;
        continue;   // Skip pages we have already copied in
      }

      last_addr = umessages[i].arg.pagefault.address;
      stat->stat_faults++;
      pagefault_event(umessages[i]);       // At this point, we know we have had a page fault.  Let's handle it.
    }
  }
}

void UserFaultHandler::pagefault_event(const struct uffd_msg& msg)
{
  void* page_begin = (void*)msg.arg.pagefault.address;
  umap_page* pm = pagebuffer->find_inmem_page_desc(page_begin);
  stringstream ss;

  if (pm != nullptr) {
    if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
      if (!pm->page_is_dirty()) {
        pm->mark_page_dirty();
        disable_wp_on_pages((uint64_t)page_begin, 1, false);
        stat->wp_messages++;
      }
      else if (msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WP) {
        struct uffdio_copy copy;
        copy.src = (uint64_t)tmppagebuf;
        copy.dst = (uint64_t)page_begin;
        copy.len = page_size;
        copy.mode = UFFDIO_COPY_MODE_WP;

        stat->stuck_wp++;

        umapdbg("EVICT WORKAROUND FOR %p\n", page_begin);

        pm->mark_page_clean();
        memcpy(tmppagebuf, page_begin, page_size);   // Save our data
        evict_page(pm);                              // Evict ourselves
        pm->set_page(page_begin);                    // Bring ourselves back in

        if (ioctl(userfault_fd, UFFDIO_COPY, &copy) == -1) {
          perror("ERROR12: ioctl(UFFDIO_COPY nowake)");
          exit(1);
        }

      }
    }
    return;
  }

  //
  // Page not in memory, read it in and (potentially) evict someone
  //
  int file_id=0;
  off_t offset=(uint64_t)page_begin - (uint64_t)_u->region;

  file_id = offset/_u->bk_files[0].data_size;   //find the file id and offset number
  offset %= _u->bk_files[0].data_size;

  if (pread(_u->bk_files[file_id].fd, tmppagebuf, page_size, offset+_u->bk_files[file_id].data_offset) == -1) {
    perror("ERROR: pread failed");
    exit(1);
  }

  if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE))
    ss << "PF(" << msg.arg.pagefault.flags << " WRITE)    (UFFDIO_COPY)       @(" << page_begin << ")";
  else
    ss << "PF(" << msg.arg.pagefault.flags << " READ)     (UFFDIO_COPY)       @(" << page_begin << ")";

  umapdbg("%s\n", ss.str().c_str());
  for (pm = pagebuffer->alloc_page_desc(page_begin); pm == nullptr; pm = pagebuffer->alloc_page_desc(page_begin)) {
    umap_page* ep = pagebuffer->get_page_desc_to_evict();
    assert(ep != nullptr);

    ss << " Evicting " << (ep->page_is_dirty() ? "Dirty" : "Clean") << "Page " << ep->get_page();
    evict_page(ep);
    pagebuffer->dealloc_page_desc(ep);
  }
  pagebuffer->add_page_desc_to_inmem(pm);

  umapdbg("%s\n", ss.str().c_str());

  struct uffdio_copy copy;
  copy.src = (uint64_t)tmppagebuf;
  copy.dst = (uint64_t)page_begin;
  copy.len = page_size;
  copy.mode = 0;

  if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
    stat->write_faults++;
    pm->mark_page_dirty();

    if (ioctl(userfault_fd, UFFDIO_COPY, &copy) == -1) {
      perror("ERROR: ioctl(UFFDIO_COPY nowake)");
      exit(1);
    }
  }
  else {
    stat->read_faults++;
    pm->mark_page_clean();

    copy.mode = UFFDIO_COPY_MODE_WP;
    if (ioctl(userfault_fd, UFFDIO_COPY, &copy) == -1) {
      perror("ERROR: ioctl(UFFDIO_COPY nowake)");
      exit(1);
    }

    assert(memcmp(tmppagebuf, page_begin, page_size) == 0);
  }
}

bool UserFaultHandler::page_is_in_umap(const void* page_begin)
{
  for ( auto it : PageBlocks )
    if (page_begin >= it.base && page_begin < (void*)((uint64_t)it.base + it.length))
      return true;
  return false;
}

void UserFaultHandler::evict_page(umap_page* pb)
{
  uint64_t* page = (uint64_t*)pb->get_page();

  if (pb->page_is_dirty()) {
    stat->dirty_evicts++;

    // Prevent further writes.  No need to do this if not dirty because WP is already on.

    enable_wp_on_pages_and_wake((uint64_t)page, 1);
    if (pwrite(_u->backingfile_fd, (void*)page, page_size, (off_t)((uint64_t)page - (uint64_t)_u->region)) == -1) {
      perror("ERROR: pwrite failed");
      assert(0);
    }
  }
  else {
    stat->clean_evicts++;
  }

  if (madvise((void*)page, page_size, MADV_DONTNEED) == -1) {
    perror("ERROR: madvise");
    assert(0);
  }

  disable_wp_on_pages((uint64_t)page, 1, true);
  pb->set_page(nullptr);
}

//
// Enabling WP always wakes up any sleeping thread that may have been faulted in the specified range.
//
// For reasons which are unknown, the kernel module interface for UFFDIO_WRITEPROTECT does not allow for the caller to submit
// UFFDIO_WRITEPROTECT_MODE_DONTWAKE when enabling WP with UFFDIO_WRITEPROTECT_MODE_WP.  UFFDIO_WRITEPROTECT_MODE_DONTWAKE is only 
// allowed when disabling WP.
//
void UserFaultHandler::enable_wp_on_pages_and_wake(uint64_t start, int64_t num_pages)
{
  struct uffdio_writeprotect wp;
  wp.range.start = start;
  wp.range.len = num_pages * page_size;
  wp.mode = UFFDIO_WRITEPROTECT_MODE_WP;

  //umapdbg("+WRITEPROTECT  (%p -- %p)\n", (void*)start, (void*)(start+((num_pages*page_size)-1))); 

  if (ioctl(userfault_fd, UFFDIO_WRITEPROTECT, &wp) == -1) {
    perror("ERROR: ioctl(UFFDIO_WRITEPROTECT Enable)");
    exit(1);
  }
}

//
// We intentionally do not wake up faulting thread when disabling WP.  This is to handle the write-fault case when the page needs to be copied in.
//
void UserFaultHandler::disable_wp_on_pages(uint64_t start, int64_t num_pages, bool do_not_awaken)
{
  struct uffdio_writeprotect wp;
  wp.range.start = start;
  wp.range.len = page_size * num_pages;
  //wp.mode = UFFDIO_WRITEPROTECT_MODE_DONTWAKE;
  wp.mode = do_not_awaken ? UFFDIO_WRITEPROTECT_MODE_DONTWAKE : 0;

  //umapdbg("-WRITEPROTECT  (%p -- %p)\n", (void*)start, (void*)(start+((num_pages*page_size)-1))); 

  if (ioctl(userfault_fd, UFFDIO_WRITEPROTECT, &wp) == -1) {
    perror("ERROR: ioctl(UFFDIO_WRITEPROTECT Disable)");
    exit(1);
  }
}

//
// umap_page_buffer class implementation
//
umap_page_buffer::umap_page_buffer(uint64_t pbuffersize) : page_buffer_size{pbuffersize}
{
  for (uint64_t i = 0; i < page_buffer_size; ++i)
    free_page_descriptors.push_front(new umap_page());
}

umap_page_buffer::~umap_page_buffer()
{
  assert(inmem_page_map.size() == 0);
  assert(inmem_page_descriptors.size() == 0);
  assert(free_page_descriptors.size() == page_buffer_size);

  for (unsigned long i = 0; i < page_buffer_size; ++i)
    delete free_page_descriptors[i];
}

umap_page* umap_page_buffer::alloc_page_desc(void* page)
{
  umap_page* p = nullptr;
  if (!free_page_descriptors.empty()) {
    p = free_page_descriptors.back();
    free_page_descriptors.pop_back();
    p->set_page(page);
  }
  return p;
}

void umap_page_buffer::dealloc_page_desc(umap_page* page_desc)
{
  page_desc->mark_page_clean();
  page_desc->set_page(nullptr);
  free_page_descriptors.push_front(page_desc);
}

void umap_page_buffer::add_page_desc_to_inmem(umap_page* page_desc)
{
  inmem_page_map[page_desc->get_page()] = page_desc;
  inmem_page_descriptors.push_front(page_desc);
}

umap_page* umap_page_buffer::get_page_desc_to_evict()
{
  umap_page* p = nullptr;
  if (!inmem_page_descriptors.empty()) {
    p = inmem_page_descriptors.back();
    inmem_page_descriptors.pop_back();
    assert(p != nullptr);
    assert(p->get_page() != nullptr);
    inmem_page_map.erase(p->get_page());
  }
  return p;
}

umap_page* umap_page_buffer::find_inmem_page_desc(void* page_addr)
{
  auto it = inmem_page_map.find(page_addr);
  return((it == inmem_page_map.end()) ? nullptr : it->second);
}

//
// umap_page class implementation
//
void umap_page::set_page(void* _p)
{ 
  page = _p;
}

//
// umap_stats implementation
//
void umap_stats::print_stats(void)
{
  cerr << stat_faults << " Faults\n"
    << read_faults << " READ Faults" << endl
    << write_faults << " WRITE Faults" << endl
    << wp_messages << " WP Messages" << endl
    << dirty_evicts << " Dirty Evictions" << endl
    << clean_evicts << " Clean Evictions" << endl
    << sigbus << " SIGBUS Errors" << endl
    << stuck_wp << " Stuck WP Workarounds" << endl
    << dropped_dups << " Dropped Duplicates" << endl;
}
