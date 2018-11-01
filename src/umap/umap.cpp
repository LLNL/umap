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
#include <fstream>
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
#include "umap/umap.h"               // API to library
#include "umap/Store.h"
#include "config.h"
#include "spindle_debug.h"

#ifndef UFFDIO_COPY_MODE_WP
#define UMAP_RO_MODE
#endif

/*
 * Note: this implementation is multi-threaded, but the data structures are
 * not shared between threads.
 */
using namespace std;

const int umap_Version_Major = UMAP_VERSION_MAJOR;
const int umap_Version_Minor = UMAP_VERSION_MINOR;
const int umap_Version_Patch = UMAP_VERSION_PATCH;

static const int UMAP_UFFD_MAX_MESSAGES = 256;
static uint64_t uffd_threads;
static uint64_t umap_buffer_size;

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
    _umap(void* _region, uint64_t _rsize, int fd, Store* _store_);
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
    Store* store;
};

class UserFaultHandler {
  friend _umap;
  friend umap_page_buffer;
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
  public:
    umap_page_buffer(UserFaultHandler* _ufh_, uint64_t pbuffersize);
    ~umap_page_buffer();
    umap_page* alloc_page_desc(void* page);
    void dealloc_page_desc( void );
    bool pages_still_present( void );

    umap_page* find_inmem_page_desc(void* page_addr);

  private:
    uint64_t page_buffer_size;
    uint64_t page_buffer_alloc_idx;
    uint64_t page_buffer_free_idx;
    uint64_t page_buffer_alloc_cnt;
    unordered_map<void*, umap_page*> inmem_page_map;
    umap_page* page_descriptor_array;
    UserFaultHandler* ufh;
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

static inline long get_max_buf_size( void )
{ static unsigned long total_mem_kb = 0;
  const unsigned long oneK = 1024;
  const unsigned long percentageToAllocate = 80;  // 80% of memory is max

  // Lazily set total_mem_kb global
  if ( ! total_mem_kb ) {
    string token;
    ifstream file("/proc/meminfo");
    while (file >> token) {
      if (token == "MemTotal:") {
        unsigned long mem;
        if (file >> mem) {
          total_mem_kb = mem;
        } else {
          cerr << "UMAP unable to determine system memory size\n";
          total_mem_kb = oneK * oneK;
        }
      }
      // ignore rest of the line
      file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
  }
  return ((total_mem_kb / (page_size / oneK)) * percentageToAllocate) / 100;
}

void* umap(void* base_addr, uint64_t region_size, int prot, int flags,
    int fd, off_t offset)
{
  return umap_ex(base_addr, region_size, prot, flags, fd, 0, nullptr);
}

void* umap_ex(void* base_addr, uint64_t region_size, int prot, int flags,
    int fd, off_t offset, Store* _store_)
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
    active_umaps[region] = new _umap{region, region_size, fd, _store_};
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
    struct umap_cfg_stats st;
    umap_cfg_get_stats(addr, &st);

    debug_printf( "\n\t"
                "Dirty Evictions: %" PRIu64 "\n\t"
                "Clean Evictions: %" PRIu64 "\n\t"
                "  Evict Victims: %" PRIu64 "\n\t"
                "    WP Messages: %" PRIu64 "\n\t"
                "    Read Faults: %" PRIu64 "\n\t"
                "   Write Faults: %" PRIu64 "\n\t"
                "  SIGBUS Errors: %" PRIu64 "\n\t"
                "       Stuck WP: %" PRIu64 "\n\t"
                "   Dropped Dups: %" PRIu64 "\n",
                st.dirty_evicts, 
                st.clean_evicts, 
                st.evict_victims,
                st.wp_messages, 
                st.read_faults, 
                st.write_faults,
                st.sigbus, 
                st.stuck_wp, 
                st.dropped_dups);

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
  if ( (umap_cfg_readenv("UMAP_UFFD_THREADS", &env_value)) ) {
    umap_cfg_set_uffdthreads(env_value);
  }

  if ( (umap_cfg_readenv("UMAP_BUFSIZE", &env_value)) ) {
    umap_cfg_set_bufsize(env_value);
  }

  if ( (umap_cfg_readenv("UMAP_PAGESIZE", &env_value)) ) {
    umap_cfg_set_pagesize(env_value);
  }
}

uint64_t umap_cfg_get_bufsize( void )
{
  return umap_buffer_size;
}

void umap_cfg_set_bufsize( uint64_t page_bufsize )
{
  long max_size = get_max_buf_size();
  long old_size = umap_buffer_size;

  if ( page_bufsize > max_size ) {
    debug_printf("Bufsize of %d larger than maximum of %d.  Setting to %d\n", 
        page_bufsize, max_size, max_size);
    umap_buffer_size = max_size;
  }
  else {
    umap_buffer_size = page_bufsize;
  }
  debug_printf("Bufsize changed from %d to %d pages\n", old_size, umap_buffer_size);
}

uint64_t umap_cfg_get_uffdthreads( void )
{
  return uffd_threads;
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
  return 0;
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
          debug_printf("SIGBUS %p (page=%p) present\n", info->si_addr, page_begin);
        else
          debug_printf("SIGBUS %p (page=%p) not present\n", info->si_addr, page_begin);
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

  umap_buffer_size = get_max_buf_size();

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
_umap::_umap( void* _region,
              uint64_t _rsize,
              int fd,
              Store* _store_) :
                region{_region}, region_size{_rsize},
                uffd_time_to_stop_working{false}, store{_store_}
{
  if ( store == nullptr )
    store = Store::make_store(_region, _rsize, page_size, fd);

  uint64_t region_pages = region_size / page_size;

  // Shrink buffer size to fit requested region if needed
  uint64_t buffer_adjusted_pages = std::min(umap_buffer_size, region_pages);

  // Shrink # of workers if there are too few pages to make it worth it.
  uint64_t num_workers = std::min(buffer_adjusted_pages, uffd_threads);

  uint64_t buffer_pages_per_worker = buffer_adjusted_pages / num_workers;
  uint64_t buffer_residual_pages = buffer_adjusted_pages % num_workers;

  uint64_t region_pages_per_worker = region_pages / num_workers;
  uint64_t region_residual_pages = region_pages % num_workers;

  stringstream ss;
  ss << "umap("
    << region << " - " << (void*)((char*)region+region_size) << ")\n\t" 
    << umap_buffer_size << " UMAP Buffer Size in Pages\n\t"
    << region_pages << " Requested Region Pages\n\t"
    << buffer_adjusted_pages << " Adjusted UMAP Buffer Size in Pages\n\t"
    << uffd_threads << " Configured Maximum UMAP Threads\n\t"
    << num_workers << " UMAP Threads Allocated\n\t"
    << buffer_pages_per_worker << " Buffer Pages per worker\n\t"
    << buffer_residual_pages << " Residual Buffer pages\n\t"
    << region_pages_per_worker << " Region Pages per worker\n\t"
    << region_residual_pages << " Risidual Buffer pages"
    << endl;
  debug_printf("%s\n", ss.str().c_str());

  try {
    uint64_t region_offset = 0;
    for (uint64_t worker = 0; worker < num_workers; ++worker) {
      umap_PageBlock pb;
      uint64_t worker_region_pages = region_pages_per_worker;
      uint64_t worker_buffer_pages = buffer_pages_per_worker;

      //
      // Distribute residual buffer pages across workers
      //
      if (buffer_residual_pages) {
        buffer_residual_pages--;
        worker_buffer_pages++;
      }

      //
      // Distribute residual buffer pages across workers
      //
      if (region_residual_pages) {
        region_residual_pages--;
        worker_region_pages++;
      }

      pb.base = (void*)((uint64_t)region + (region_offset * page_size));
      pb.length = worker_region_pages * page_size;

      vector<umap_PageBlock> segs{ pb };

      ufault_handlers.push_back( new UserFaultHandler{this, segs, worker_buffer_pages} );
      region_offset += worker_region_pages;
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
      pagebuffer{ new umap_page_buffer{this, _pbuf_size} }
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

    debug_printf2("Register %d Pages from: %p - %p\n", 
        (seg.length / page_size), seg.base, 
        (void*)((uint64_t)seg.base + (uint64_t)(seg.length-1)));

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

  if (pm != nullptr) {
#ifndef UMAP_RO_MODE
    if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
      if (!pm->page_is_dirty()) {
        pm->mark_page_dirty();
        disable_wp_on_pages((uint64_t)page_begin, 1, false);
        stat->wp_messages++;
        debug_printf2("Present page written, marking %p dirty\n", page_begin);
      }
      else if (msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WP) {
        struct uffdio_copy copy;
        copy.src = (uint64_t)copyin_buf;
        copy.dst = (uint64_t)page_begin;
        copy.len = page_size;
        copy.mode = 0;  // No WP

        stat->stuck_wp++;

        pm->mark_page_clean();
        memcpy(copyin_buf, page_begin, page_size);   // Save our data
        evict_page(pm);                              // Evict ourselves
        pm->set_page(page_begin);                    // Bring ourselves back in
        pm->mark_page_dirty();                       // Will be dirty when write retries

        if (ioctl(userfault_fd, UFFDIO_COPY, &copy) == -1) {
          perror("ERROR WP Workaround: ioctl(UFFDIO_COPY)");
          exit(1);
        }
        debug_printf2("Present page stuck, EVICT WORKAROUND %p\n", page_begin);
      }
    }
#else
    if ( msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WRITE ) {
      assert("Write operation not allowed without WP support" && 0);
    }
#endif
    else {
      debug_printf2("Spurious fault for page %p which is already present\n",
          page_begin);
    }
    return;
  }

  //
  // Page not present, read it in and (potentially) evict someone
  //
  off_t offset=(uint64_t)page_begin - (uint64_t)_u->region;

  if (_u->store->read_from_store(copyin_buf, page_size, offset) == -1) {
    perror("ERROR: read_from_store failed");
    exit(1);
  }

  /*
   * Keep trying to obtain a free page descriptor until we get one..
   */
  for ( pm = pagebuffer->alloc_page_desc(page_begin); 
        pm == nullptr; 
        pm = pagebuffer->alloc_page_desc(page_begin))
  {
    pagebuffer->dealloc_page_desc();
  }

  struct uffdio_copy copy;
  copy.src = (uint64_t)copyin_buf;
  copy.dst = (uint64_t)page_begin;
  copy.len = page_size;
  copy.mode = 0;

#ifndef UMAP_RO_MODE
  if (msg.arg.pagefault.flags & (UFFD_PAGEFAULT_FLAG_WP | UFFD_PAGEFAULT_FLAG_WRITE)) {
    debug_printf3("Write Fault: Copying in dirty page %p\n", page_begin);
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
    debug_printf3("Read Fault: Copying in page %p\n", page_begin);
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

void UserFaultHandler::flushbuffers( void )
{
  while (pagebuffer->pages_still_present() == true)
    pagebuffer->dealloc_page_desc();
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

  stat->evict_victims++;
  if (pb->page_is_dirty()) {
#ifdef UMAP_RO_MODE
    assert("Dirty page found when running in RO mode" && 0);
#else
    stat->dirty_evicts++;

    // Prevent further writes.  No need to do this if not dirty because WP is already on.

    enable_wp_on_pages_and_wake((uint64_t)page, 1);
    if (_u->store->write_to_store((char*)page, page_size, (off_t)((uint64_t)page - (uint64_t)_u->region)) == -1) {
      perror("ERROR: write_to_store failed");
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
  wp.mode = do_not_awaken ? UFFDIO_WRITEPROTECT_MODE_DONTWAKE : 0;

  if (ioctl(userfault_fd, UFFDIO_WRITEPROTECT, &wp) == -1) {
    perror("ERROR: ioctl(UFFDIO_WRITEPROTECT Disable)");
    exit(1);
  }
}
#endif

//
// umap_page_buffer class implementation
//
umap_page_buffer::umap_page_buffer(UserFaultHandler* _ufh_, uint64_t pbuffersize)
  : ufh{_ufh_}, page_buffer_size{pbuffersize}, page_buffer_alloc_idx{0}, 
    page_buffer_free_idx{0}, page_buffer_alloc_cnt{0}
{
  page_descriptor_array = (umap_page *)calloc(page_buffer_size, sizeof(umap_page));
}

umap_page_buffer::~umap_page_buffer()
{
  assert(inmem_page_map.size() == 0);
  assert(page_buffer_alloc_cnt == 0);

  free(page_descriptor_array);
}

umap_page* umap_page_buffer::alloc_page_desc(void* page)
{
  if ( page_buffer_alloc_cnt < page_buffer_size ) {
    umap_page* p = page_descriptor_array + page_buffer_alloc_idx;
    page_buffer_alloc_idx = (page_buffer_alloc_idx + 1) % page_buffer_size;
    page_buffer_alloc_cnt++;
    p->set_page(page);
    inmem_page_map[page] = p;
    debug_printf3("%p allocated for %p, free idx=%d alloc idx=%d cnt=%d\n",
        p, page, page_buffer_free_idx, page_buffer_alloc_idx, page_buffer_alloc_cnt);
    return p;
  }
  return nullptr;
}

bool umap_page_buffer::pages_still_present( void )
{
  return page_buffer_alloc_cnt != 0;
}

void umap_page_buffer::dealloc_page_desc( void )
{
  umap_page* p = page_buffer_alloc_cnt ? 
                    page_descriptor_array + page_buffer_free_idx : nullptr;

  if ( p != nullptr ) {
    debug_printf3("%p freed for %p, free idx=%d alloc idx=%d cnt=%d\n",
        p, p->get_page(), page_buffer_alloc_idx, 
        page_buffer_free_idx, page_buffer_alloc_cnt);
    page_buffer_free_idx = (page_buffer_free_idx + 1) % page_buffer_size;
    page_buffer_alloc_cnt--;
    inmem_page_map.erase(p->get_page());

    ufh->evict_page(p);
    p->mark_page_clean();
    p->set_page(nullptr);
  }
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
