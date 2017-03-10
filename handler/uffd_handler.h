#ifndef UFFD_HANDLER_H
#define UFFD_HANDLER_H
#include <linux/userfaultfd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <poll.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

struct params {
  int uffd;
  long page_size;
  int fd;
};

static void *ufdd_handler(void *arg);

#endif // UFFD_HANDLER_H
