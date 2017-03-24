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

extern volatile int stop_uffd_handler;

int uffd_init(void *region, long page_size, long num_pages);
void *uffd_handler(void *arg);
int uffd_finalize(void *region, int uffd, long page_size, long num_pages);
long get_pagesize(void);

#endif // UFFD_HANDLER_H
