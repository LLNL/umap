#include <unistd.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <semaphore.h>
#include "socket.hpp"

ssize_t sock_fd_write(int sock, void *buf, ssize_t buflen, int fd) {
  ssize_t     size;
  struct msghdr   msg;
  struct iovec    iov;
  union cmsg_u cmsgu;
  struct cmsghdr  *cmsg;

  iov.iov_base = buf;
  iov.iov_len = buflen;

  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  if (fd != -1) {
    msg.msg_control = cmsgu.control;
    msg.msg_controllen = sizeof(cmsgu.control);

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len = CMSG_LEN(sizeof (int));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;

    printf ("passing fd %d\n", fd);
    *((int *) CMSG_DATA(cmsg)) = fd;
  } else {
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    printf ("not passing fd\n");
  }

  size = sendmsg(sock, &msg, 0);

  if (size < 0) {
    perror ("sendmsg");
  }
  return size;
}

ssize_t sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd) {
  ssize_t     size;

  if (fd) {
    struct msghdr   msg;
    struct iovec    iov;
    union cmsg_u cmsgu;
    struct cmsghdr  *cmsg;

    iov.iov_base = buf;
    iov.iov_len = bufsize;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgu.control;
    msg.msg_controllen = sizeof(cmsgu.control);
    size = recvmsg (sock, &msg, 0);
    if (size < 0) {
      perror ("recvmsg");
      exit(1);
    }
    cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
      if (cmsg->cmsg_level != SOL_SOCKET) {
        fprintf (stderr, "invalid cmsg_level %d\n", cmsg->cmsg_level);
        exit(1);
      }
      if (cmsg->cmsg_type != SCM_RIGHTS) {
        fprintf (stderr, "invalid cmsg_type %d\n", cmsg->cmsg_type);
        exit(1);
      }

      *fd = *((int *) CMSG_DATA(cmsg));
      printf ("received fd %d\n", *fd);
    } else {
      *fd = -1;
    }
  } else {
    size = read (sock, buf, bufsize);
    if (size < 0) {
      perror("read");
      exit(1);
    }
  }
  return size;
}

int sock_recv(int sock, char* buf, uint64_t sz) {
  int status = 0;
  memset(buf, 0, sz);
  if ((status = read(sock, buf, sz)) < 0) {
    perror("reading stream message");
    return 1;
  } else if (status == 0) {
    printf("Ending connection\n");
    return 1;
  }
  return 0;
}

int setup_uds_connection(int *fd, const char *sock_path){
  struct sockaddr_un sock_addr;
  
  *fd = socket(AF_UNIX, SOCK_STREAM, 0);
  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sun_family = AF_UNIX;
  strncpy(sock_addr.sun_path, sock_path, sizeof(sock_addr.sun_path));
  if (connect(*fd, (struct sockaddr *) &sock_addr, sizeof(sock_addr)) == -1) {
    close(*fd);
    perror("connect");
    return -1;
  }
  return 0;
}
