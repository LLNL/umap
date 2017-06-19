#include <linux/userfaultfd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef UFFD_HANDLER_H
#define UFFD_HANDLER_H

// Uncomment the following line to enable tracing (to circular buffer in mem)
#define ENABLE_FAULT_TRACE_BUFFER 1

#ifdef ENABLE_FAULT_TRACE_BUFFER
#define TRACE(_pb_, _ft_, _et_) pa_trace((uint64_t)_pb_, _ft_, _et_)
#else
#define TRACE(_pb_, _ft_, _et_) ;
#endif // ENABLE_FAULT_TRACE_BUFFER

typedef struct params {
  int uffd;
  void* base_addr;
  long pagesize;
  int bufsize;
  int faultnum;
  int fd;
} params_t;

typedef struct pagebuffer {
    void* page;
    bool dirty;
} pagebuffer_t;

enum fault_types {
    ft_NA=-1,
    ft_read=0,
    ft_write=1,
    ft_wp=2
};

enum evict_types {
    et_NA=-1,
    et_none=0,
    et_clean=1,
    et_dirty=2
};

typedef struct {
    int trace_seq;
    void* page;
    enum fault_types ftype;
    enum evict_types etype;
} page_activity_trace_t;

#ifdef __cplusplus
extern "C" {
#endif
    extern volatile int stop_uffd_handler;
    long get_pagesize(void);
    int uffd_init(void*, long, long);
    void *uffd_handler(void*);
    int uffd_finalize(void*, long);
#ifdef __cplusplus
}
#endif

#ifdef ENABLE_FAULT_TRACE_BUFFER
void pa_trace(uint64_t, enum fault_types, enum evict_types);
#endif // ENABLE_FAULT_TRACE_BUFFER

void enable_wp_on_pages_and_wake(int, uint64_t, int64_t, int64_t);
void disable_wp_on_pages(int, uint64_t, int64_t, int64_t);
void evict_page(params_t*, pagebuffer_t *);

#endif // UFFD_HANDLER_H
