/* This file is part of UMAP.
 *
 * For copyright information see the COPYRIGHT file in the top level directory,
 * or at https://github.com/LLNL/umap/blob/master/COPYRIGHT.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (as published by the Free
 * Software Foundation) version 2.1 dated February 1999.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU
 * Lesser General Public License for more details.  You should have received a
 * copy of the GNU Lesser General Public License along with this program;
 * if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite
 * 330, Boston, MA 02111-1307 USA
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <iostream>
#include <cstdint>
#include <cinttypes>
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
#include "config.h"
#include "spindle_debug.h"

#ifndef UFFDIO_COPY_MODE_WP
#define UMAP_RO_MODE
#endif

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
    _umap(void* _region, uint64_t _rsize, umap_pstore_read_f_t _ps_read, umap_pstore_write_f_t _ps_write);
    ~_umap();

    static inline void* UMAP_PAGE_BEGIN(const void* a) {
      return (void*)((uint64_t)a & ~(page_size-1));
    }

    void flushbuffers( void );

    vector<UserFaultHandler*> ufault_handlers;

  private:
    void* region;
    uint64_t region_size;
    bool uffd_time_to_stop_working;
    umap_pstore_read_f_t pstore_read;
    umap_pstore_write_f_t pstore_write;
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
    void flushbuffers( void );
    void resetstats( void );

    umap_stats* stat;
  private:
    _umap* _u;
    vector<umap_PageBlock> PageBlocks;
    uint64_t pbuf_size;
    umap_page_buffer* pagebuffer;
    vector<struct uffd_msg> umessages;

    int userfault_fd;
    char* copyin_buf;
    thread* uffd_worker;

    void evict_page(umap_page* page);
    void uffd_handler(void);
    void pagefault_event(const struct uffd_msg& msg);
#ifndef UMAP_RO_MODE
    void enable_wp_on_pages_and_wake(uint64_t, int64_t);
    void disable_wp_on_pages(uint64_t, int64_t, bool);
#endif
};

class umap_stats {
  public:
    umap_stats():
      dirty_evicts{0},
      clean_evicts{0},
      evict_victims{0},
      wp_messages{0},
      read_faults{0},
      write_faults{0},
      sigbus{0},
      stuck_wp{0},
      dropped_dups{0}
      {};

    uint64_t dirty_evicts;
    uint64_t clean_evicts;
    uint64_t evict_victims;
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
    vector<umap_page*> free_page_descriptors;
    deque<umap_page*> inmem_page_descriptors;
    unordered_map<void*, umap_page*> inmem_page_map;
    umap_page* page_descriptor_array;
};

struct umap_page {
    bool page_is_dirty() { return dirty; }
    void mark_page_dirty() { dirty = true; }
    void mark_page_clean() { dirty = false; }
    void* get_page(void) { return page; }
    void set_page(void* _p);
    void* page;
    bool dirty;
};

static unordered_map<void*, _umap*> active_umaps;

static inline bool required_uffd_features_present(int fd)
{
  struct uffdio_api uffdio_api = {
    .api = UFFD_API,
#ifdef UMAP_RO_MODE
    .features = 0
#else
    .features = UFFD_FEATURE_PAGEFAULT_FLAG_WP
#endif
  };

  if (ioctl(fd, UFFDIO_API, &uffdio_api) == -1) {
    perror("ERROR: UFFDIO_API Failed: ");
    return false;
  }

#ifndef UMAP_RO_MODE
  if ( !(uffdio_api.features & UFFD_FEATURE_PAGEFAULT_FLAG_WP) ) {
    cerr << "UFFD Compatibilty Check - unsupported userfaultfd WP\n";
    return false;
  }
#endif

  return true;
}

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

  if ( ! required_uffd_features_present(fd) )
    exit(1);

  close(fd);

  return 0;
}

void* umap(void* base_addr, uint64_t region_size, int prot, int flags, umap_pstore_read_f_t _ps_read, umap_pstore_write_f_t _ps_write)
{
  if (check_uffd_compatibility() < 0)
    return NULL;

  if ( (region_size % page_size) ) {
    cerr << "UMAP: Region size " << region_size << " is not a multple of page_size (" << page_size << ")\n";
    return NULL;
  }

  if (!(flags & UMAP_PRIVATE) || flags & ~(UMAP_PRIVATE|UMAP_FIXED)) {
    cerr << "umap: Invalid flags: " << hex << flags << endl;
    return UMAP_FAILED;
  }

  void* region = mmap(base_addr, region_size, prot, flags | (MAP_ANONYMOUS | MAP_NORESERVE), -1, 0);

  if (region == MAP_FAILED) {
    perror("ERROR: mmap failed: ");
    return UMAP_FAILED;
  }

  try {
    active_umaps[region] = new _umap{region, region_size, _ps_read, _ps_write};
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

uint64_t* umap_cfg_readenv(const char* env, uint64_t* val) {
  // return a pointer to val on success, null on failure
  char* val_ptr = 0;
  if ( (val_ptr = getenv(env)) ) {
    uint64_t env_val = 0;
    if (sscanf(val_ptr, "%" PRIu64, &env_val)) {
      *val = env_val;
      return val;
    }
  }
  return 0;
}

void umap_cfg_getenv( void ) {
  uint64_t env_value = 0;
  if ( (umap_cfg_readenv("UMAP_BUFSIZE", &env_value)) ) {
    umap_cfg_set_bufsize(env_value);
  }

  if ( (umap_cfg_readenv("UMAP_UFFD_THREADS", &env_value)) ) {
    umap_cfg_set_uffdthreads(env_value);
  }

  if ( (umap_cfg_readenv("UMAP_PAGESIZE", &env_value)) ) {
    umap_cfg_set_pagesize(env_value);
  }
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

uint64_t umap_cfg_get_uffdthreads( void )
{
  return (uint64_t)uffd_threads;
}

void umap_cfg_set_uffdthreads( uint64_t numthreads )
{
  uffd_threads = numthreads;
}

void umap_cfg_flush_buffer( void* region )
{
  auto it = active_umaps.find(region);

  if (it != active_umaps.end())
    it->second->flushbuffers();
}

int umap_cfg_get_pagesize()
{
  return page_size;
}

int umap_cfg_set_pagesize( long psize )
{
  long sys_psize = sysconf(_SC_PAGESIZE);

  /*
   * Must be multiple of system page size
   */
  if ( psize % sys_psize ) {
    cerr << "Specified page size (" << psize << ") must be a multiple of system page size (" << sys_psize << ")\n";
    return -1;
  }

  debug_printf("Adjusting page size from %d to %d\n", page_size, psize);

  page_size = psize;
}

void umap_cfg_get_stats(void* region, struct umap_cfg_stats* stats)
{
  auto it = active_umaps.find(region);

  if (it != active_umaps.end()) {
    stats->dirty_evicts = 0;
    stats->clean_evicts = 0;
    stats->evict_victims = 0;
    stats->wp_messages = 0;
    stats->read_faults = 0;
    stats->write_faults = 0;
    stats->sigbus = 0;
    stats->stuck_wp = 0;
    stats->dropped_dups = 0;

    for ( auto handler : it->second->ufault_handlers ) {
      stats->dirty_evicts += handler->stat->dirty_evicts;
      stats->clean_evicts += handler->stat->clean_evicts;
      stats->evict_victims += handler->stat->evict_victims;
      stats->wp_messages += handler->stat->wp_messages;
      stats->read_faults += handler->stat->read_faults;
      stats->write_faults += handler->stat->write_faults;
      stats->sigbus += handler->stat->sigbus;
      stats->stuck_wp += handler->stat->stuck_wp;
      stats->dropped_dups += handler->stat->dropped_dups;
    }
  }
}

void umap_cfg_reset_stats(void* region)
{
  auto it = active_umaps.find(region);

  if (it != active_umaps.end()) {
    for ( auto handler : it->second->ufault_handlers )
      handler->resetstats();
  }
}

//
// Signal Handlers
//
static struct sigaction saved_sa;

void sighandler(int signum, siginfo_t *info, void* buf)
{
  if (signum != SIGBUS) {
    err_printf("Unexpected signal: %d received\n", signum);
    exit(1);
  }

  //assert("UMAP: SIGBUS Error Unexpected" && 0);

  void* page_begin = _umap::UMAP_PAGE_BEGIN(info->si_addr);

  for (auto it : active_umaps) {
    for (auto ufh : it.second->ufault_handlers) {
      if (ufh->page_is_in_umap(page_begin)) {
        ufh->stat->sigbus++;

        if (ufh->get_pagebuffer()->find_inmem_page_desc(page_begin) != nullptr)
          err_printf("SIGBUS %p (page=%p) ALREADY IN UMAP PAGE_BUFFER\n", info->si_addr, page_begin);
        else
          err_printf("SIGBUS %p (page=%p) Not currently in umap buffer\n", info->si_addr, page_begin);
        return;
      }
    }
  }
  err_printf("SIGBUS %p (page=%p) ADDRESS OUTSIDE OF UMAP RANGE\n", info->si_addr, page_begin);
  assert("UMAP: SIGBUS for out of range address" && 0);
}

void __attribute ((constructor)) init_umap_lib( void )
{
  struct sigaction act;

  LOGGING_INIT;

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

  umap_cfg_getenv();
  LOGGING_FINI;
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
_umap::_umap(void* _region, uint64_t _rsize, umap_pstore_read_f_t _ps_read, umap_pstore_write_f_t _ps_write)
    : region{_region}, region_size{_rsize}, uffd_time_to_stop_working{false}, pstore_read{_ps_read}, pstore_write{_ps_write}
{
  uint64_t pages_in_region = region_size / page_size;
  uint64_t pages_per_block = pages_in_region < UMAP_PAGES_PER_BLOCK ? pages_in_region : UMAP_PAGES_PER_BLOCK;
  uint64_t page_blocks = pages_in_region / pages_per_block;
  uint64_t additional_pages_for_last_block = pages_in_region % pages_per_block;

  uint64_t num_workers = page_blocks < uffd_threads ? page_blocks : uffd_threads;
  uint64_t page_blocks_per_worker = page_blocks / num_workers;
  uint64_t additional_blocks_for_last_worker = page_blocks % num_workers;

  stringstream ss;
  ss << "umap("
    << region << " - " << (void*)((char*)region+region_size)
    << ") " << pages_in_region << " region pages, "
    << pages_per_block << " pages per block, "
    << page_blocks  << " page blocks, "
    << additional_pages_for_last_block << " additional pages for last block, "
    << num_workers << " workers, "
    << page_blocks_per_worker << " page blocks per worker, "
    << additional_blocks_for_last_worker << " additional blocks for last worker"
    << endl;
  debug_printf("%s\n", ss.str().c_str());

  try {
    for (uint64_t worker = 0; worker < num_workers; ++worker) {
      umap_PageBlock pb;

      pb.base = (void*)((uint64_t)region + (worker * page_blocks_per_worker * pages_per_block * page_size));
      pb.length = page_blocks_per_worker * pages_per_block * page_size;

      // If I am the last worker, deal with any residual blocks and pages
      if (worker == (num_workers-1)) {
        if (additional_blocks_for_last_worker)
          pb.length += (additional_blocks_for_last_worker * pages_per_block * page_size);

        if (additional_pages_for_last_block)
          pb.length += (additional_pages_for_last_block * page_size);
      }

      vector<umap_PageBlock> segs{ pb };

      // TODO - Find a way to more fairly distribute buffer to handlers
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

void _umap::flushbuffers( void )
{
  for ( auto handler : ufault_handlers )
    handler->flushbuffers();
}

_umap::~_umap(void)
{
  for ( auto handler : ufault_handlers )
    handler->stop_uffd_worker();

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

  if (posix_memalign((void**)&copyin_buf, (uint64_t)page_size, (page_size * 2))) {
    cerr << "ERROR: posix_memalign: failed\n";
    exit(1);
  }

  if (copyin_buf == nullptr) {
    cerr << "Unable to allocate " << page_size << " bytes for temporary buffer\n";
    exit(1);
  }

  if ((userfault_fd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK)) < 0) {
    perror("ERROR: userfaultfd syscall not available in this kernel");
    throw -1;
  }

  if ( ! required_uffd_features_present(userfault_fd) )
    exit(1);

  for ( auto seg : PageBlocks ) {
    struct uffdio_register uffdio_register = {
      .range = {.start = (uint64_t)seg.base, .len = seg.length},
#ifndef UMAP_RO_MODE
      .mode = UFFDIO_REGISTER_MODE_MISSING | UFFDIO_REGISTER_MODE_WP
#else
      .mode = UFFDIO_REGISTER_MODE_MISSING
#endif
    };

    debug_printf("Register %p - %p\n", seg.base, (void*)((uint64_t)seg.base + (uint64_t)(seg.length-1)));

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

  free(copyin_buf);
  delete pagebuffer;
  delete stat;
  delete uffd_worker;
}

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
      flushbuffers();
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

    if ( !(pollfd[0].revents & POLLIN) )
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
    debug_printf3("%s\n", ss.str().c_str());
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
#ifndef UMAP_RO_MODE
    if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
      if (!pm->page_is_dirty()) {
        ss << "PF(" << msg.arg.pagefault.flags << " WP)    (DISABLE_WP)       @(" << page_begin << ")";
        debug_printf3("%s\n", ss.str().c_str());

        pm->mark_page_dirty();
        disable_wp_on_pages((uint64_t)page_begin, 1, false);
        stat->wp_messages++;
      }
      else if (msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WP) {
        struct uffdio_copy copy;
        copy.src = (uint64_t)copyin_buf;
        copy.dst = (uint64_t)page_begin;
        copy.len = page_size;
        copy.mode = 0;  // No WP

        stat->stuck_wp++;

        debug_printf3("EVICT WORKAROUND FOR %p\n", page_begin);

        pm->mark_page_clean();
        memcpy(copyin_buf, page_begin, page_size);   // Save our data
        evict_page(pm);                              // Evict ourselves
        pm->set_page(page_begin);                    // Bring ourselves back in
        pm->mark_page_dirty();                       // Will be dirty when write retries

        if (ioctl(userfault_fd, UFFDIO_COPY, &copy) == -1) {
          perror("ERROR WP Workaround: ioctl(UFFDIO_COPY)");
          exit(1);
        }
      }
    }
#else
    if ( msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WRITE ) {
      assert("Write operation not allowed without WP support" && 0);
    }
#endif
    return;
  }

  //
  // Page not in memory, read it in and (potentially) evict someone
  //
  off_t offset=(uint64_t)page_begin - (uint64_t)_u->region;

  if (_u->pstore_read(_u->region, copyin_buf, page_size, offset) == -1) {
    perror("ERROR: pstore_read failed");
    exit(1);
  }

#ifndef UMAP_RO_MODE
  if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE))
    ss << "PF(" << msg.arg.pagefault.flags << " WRITE)    (UFFDIO_COPY)       @(" << page_begin << ")";
  else
    ss << "PF(" << msg.arg.pagefault.flags << " READ)     (UFFDIO_COPY)       @(" << page_begin << ")";
#else
  if (msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WRITE)
    ss << "PF(" << msg.arg.pagefault.flags << " WRITE)    (UFFDIO_COPY)       @(" << page_begin << ")";
  else
    ss << "PF(" << msg.arg.pagefault.flags << " READ)     (UFFDIO_COPY)       @(" << page_begin << ")";
#endif

  for (pm = pagebuffer->alloc_page_desc(page_begin); pm == nullptr; pm = pagebuffer->alloc_page_desc(page_begin)) {
    umap_page* ep = pagebuffer->get_page_desc_to_evict();
    assert(ep != nullptr);

    ss << " Evicting " << (ep->page_is_dirty() ? "Dirty" : "Clean") << "Page " << ep->get_page();
    stat->evict_victims++;
    evict_page(ep);
    pagebuffer->dealloc_page_desc(ep);
  }
  pagebuffer->add_page_desc_to_inmem(pm);

  debug_printf3("%s\n", ss.str().c_str());

  struct uffdio_copy copy;
  copy.src = (uint64_t)copyin_buf;
  copy.dst = (uint64_t)page_begin;
  copy.len = page_size;
  copy.mode = 0;

#ifndef UMAP_RO_MODE
  if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
    stat->write_faults++;
    pm->mark_page_dirty();

    if (ioctl(userfault_fd, UFFDIO_COPY, &copy) == -1) {
      perror("ERROR: ioctl(UFFDIO_COPY nowake)");
      exit(1);
    }
  }
#else
  if (msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WRITE) {
    assert("Write operation not allowed without WP support" && 0);
  }
#endif
  else {
    stat->read_faults++;
    pm->mark_page_clean();

#ifndef UMAP_RO_MODE
    copy.mode = UFFDIO_COPY_MODE_WP;
#else
    copy.mode = 0;
#endif
    if (ioctl(userfault_fd, UFFDIO_COPY, &copy) == -1) {
      perror("ERROR: ioctl(UFFDIO_COPY nowake)");
      exit(1);
    }

    assert(memcmp(copyin_buf, page_begin, page_size) == 0);
  }
}

bool UserFaultHandler::page_is_in_umap(const void* page_begin)
{
  for ( auto it : PageBlocks )
    if (page_begin >= it.base && page_begin < (void*)((uint64_t)it.base + it.length))
      return true;
  return false;
}

// TODO: make this thread-safe (it isn't currently)
void UserFaultHandler::flushbuffers( void )
{
  for (umap_page* ep = pagebuffer->get_page_desc_to_evict(); ep != nullptr; ep = pagebuffer->get_page_desc_to_evict()) {
    evict_page(ep);
    pagebuffer->dealloc_page_desc(ep);
  }
}

void UserFaultHandler::resetstats( void )
{
  stat->dirty_evicts = 0;
  stat->clean_evicts = 0;
  stat->evict_victims = 0;
  stat->wp_messages = 0;
  stat->read_faults = 0;
  stat->write_faults = 0;
  stat->sigbus = 0;
  stat->stuck_wp = 0;
  stat->dropped_dups = 0;
}

void UserFaultHandler::evict_page(umap_page* pb)
{
  uint64_t* page = (uint64_t*)pb->get_page();

  if (pb->page_is_dirty()) {
#ifdef UMAP_RO_MODE
    assert("Dirty page found when running in RO mode" && 0);
#else
    stat->dirty_evicts++;

    // Prevent further writes.  No need to do this if not dirty because WP is already on.

    enable_wp_on_pages_and_wake((uint64_t)page, 1);
    if (_u->pstore_write(_u->region, (char*)page, page_size, (off_t)((uint64_t)page - (uint64_t)_u->region)) == -1) {
      perror("ERROR: pstore_write failed");
      assert(0);
    }
#endif
  }
  else {
    stat->clean_evicts++;
  }

  if (madvise((void*)page, page_size, MADV_DONTNEED) == -1) {
    perror("ERROR: madvise");
    assert(0);
  }

  pb->set_page(nullptr);
}

#ifndef UMAP_RO_MODE
//
// Enabling WP always wakes up blocked faulting threads that may have been faulted in the specified range.
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

  debug_printf3("+WRITEPROTECT  (%p -- %p)\n", (void*)start, (void*)(start+((num_pages*page_size)-1)));

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

  //debug_printf3("-WRITEPROTECT  (%p -- %p)\n", (void*)start, (void*)(start+((num_pages*page_size)-1)));

  if (ioctl(userfault_fd, UFFDIO_WRITEPROTECT, &wp) == -1) {
    perror("ERROR: ioctl(UFFDIO_WRITEPROTECT Disable)");
    exit(1);
  }
}
#endif

//
// umap_page_buffer class implementation
//
umap_page_buffer::umap_page_buffer(uint64_t pbuffersize) : page_buffer_size{pbuffersize}
{
  free_page_descriptors.reserve(page_buffer_size);
  page_descriptor_array = (umap_page *)calloc(page_buffer_size, sizeof(umap_page));

  for (uint64_t i = 0; i < page_buffer_size; ++i)
    free_page_descriptors.push_back(page_descriptor_array + i);
}

umap_page_buffer::~umap_page_buffer()
{
  assert(inmem_page_map.size() == 0);
  assert(inmem_page_descriptors.size() == 0);
  assert(free_page_descriptors.size() == page_buffer_size);

  //for (unsigned long i = 0; i < page_buffer_size; ++i)
    //free_page_descriptors.pop_back();

  free(page_descriptor_array);
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
  free_page_descriptors.push_back(page_desc);
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
