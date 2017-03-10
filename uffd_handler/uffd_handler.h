#ifndef UFFD_HANDLER_H
#define UFFD_HANDLER_H

struct params {
  int uffd;
  long page_size;
  int faultnum;
  int fd;
};

void *ufdd_init();
void *ufdd_handler(void *arg);
long get_page_size(void);

#endif // UFFD_HANDLER_H
