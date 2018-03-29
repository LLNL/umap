#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define BSIZE (256*1024*1024)

void mv(char* ifile, char* ofile, int remove_old, char* buffer)
{
  struct stat st;
  int ifd, ofd;

  printf("Processing %s\n", ofile);
  if (buffer == NULL) {
    fprintf(stderr, "Could not allocated %d bytes\n", BSIZE);
    _exit(1);
  }

  if (stat(ifile, &st)) {
    fprintf(stderr, "Could not stat %s: %s\n", ifile, strerror(errno));
    _exit(1);
  }

  if ((ifd = open(ifile, (O_RDONLY | O_LARGEFILE))) < 0) {
    fprintf(stderr, "Could not open %s: %s\n", ifile, strerror(errno));
    _exit(1);
  }

  if ((ofd = open(ofile, (O_RDWR | O_CREAT | O_LARGEFILE | O_TRUNC | O_DIRECT), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))) < 0) {
    fprintf(stderr, "Could not create %s: %s\n", ofile, strerror(errno));
    _exit(1);
  }

  if (lseek(ifd, 2880, SEEK_SET) == (off_t)-1) {
    fprintf(stderr, "Could not set initial seek in %s: %s\n", ifile, strerror(errno));
    _exit(1);
  }

  ssize_t tsize = 0;
  for (ssize_t rv = BSIZE; rv == BSIZE; ) {
    if ((rv = read(ifd, buffer, BSIZE)) < 0) {
      fprintf(stderr, "Read failed in %s: %s\n", ifile, strerror(errno));
      _exit(1);
    }

    if (rv < BSIZE)
      break;

    if (write(ofd, buffer, rv) != rv) {
      fprintf(stderr, "Read failed in %s: %s\n", ifile, strerror(errno));
      _exit(1);
    }
    tsize += rv;
  }

  close(ifd);
  close(ofd);

  if ( remove_old ) {
    if (unlink(ifile) < 0) {
      fprintf(stderr, "Read failed in %s: %s\n", ifile, strerror(errno));
      _exit(1);
    }
  }

  printf("Wrote %zu bytes to %s\n", tsize, ofile);
}

int main(int ac, char** av)
{
  char ifilename[256];
  char ofilename[256];
  char* buffer = (char*)aligned_alloc(4096, BSIZE);

  for (int i = 1; i <= 100; i++) {
    sprintf(ifilename, "asteroid_sim_epoch%d.fits", i);
    sprintf(ofilename, "asteroid_sim_epoch%d.data", i);
    mv(ifilename, ofilename, (i != 1), buffer);
  }

  free(buffer);
  return 0;
}
