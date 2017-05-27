#include <stdbool.h>

#ifndef UFFD_HANDLER_H
#define UFFD_HANDLER_H

typedef struct params {
  int uffd;
  void *base_addr;
  long pagesize;
  int bufsize;
  int faultnum;
  int fd;
} params_t;

typedef struct pagebuffer {
    void *page;
    bool dirty;
} pagebuffer_t;


#ifdef __cplusplus
extern "C" volatile int stop_uffd_handler;
#else
extern volatile int stop_uffd_handler;
#endif

int uffd_init(void*, long, long);
void *uffd_handler(void*);
void enable_wp_on_pages(int, uint64_t, int64_t, int64_t);
void disable_wp_on_pages(int, uint64_t, int64_t, int64_t);
int uffd_finalize(void*, long);
long get_pagesize(void);
void evict_page(params_t*, pagebuffer_t*);
void print_uffd_msg_info(struct uffd_msg*);

#endif // UFFD_HANDLER_H
