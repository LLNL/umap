#ifndef UFFD_SOCKET_H
#define UFFD_SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

union cmsg_u {
  struct cmsghdr  cmsghdr;
  char   control[CMSG_SPACE(sizeof (int))];
};

ssize_t sock_fd_write(int sock, void *buf, ssize_t buflen, int fd);
ssize_t sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd);
int sock_recv(int sock, char* buf, uint64_t sz);
int setup_uds_connection(int *fd, const char *sock_path);
#endif
