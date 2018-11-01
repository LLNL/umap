/*
This file was taken from Spindle and made part of Umpire.  The Spindle and
umpire boilerplate is included below.

This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at
https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation)
version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms
and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the
GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

This file is part of Spindle.  For copyright information see the COPYRIGHT
file in the top level directory, or at
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms
and conditions of the GNU Lesser General Public License for more details.  You should
have received a copy of the GNU Lesser General Public License along with this
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/
#define _GNU_SOURCE

#include "config.h"

#if defined(UMAP_DEBUG_LOGGING)
#include "spindle_debug.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <execinfo.h>

static char spindle_log_daemon_name[] = "../libexec/umap_logd";
static char spindle_log_daemon_name2[] = "../../src/umap/umap_logd";

static int debug_fd = -1;
static char *tempdir;
static int run_local_only = 1;    // Don't use sockets
static char *debug_location;

FILE *spindle_debug_output_f;
char *spindle_debug_name = "umap";
int spindle_debug_prints;

//Timeout in tenths of a second
#define SPAWN_TIMEOUT 300
#define CONNECT_TIMEOUT 100

extern int spindle_mkdir(char *orig_path);

int fileExists(char *name)
{
   struct stat buf;
   return (stat(name, &buf) != -1);
}

#include <inttypes.h>
#define MAX_EXE_PATH_STR_SIZE 4096 + 1
static void getProgramAndPath( char** fpath, char** ppath, char**  pname)
{
  static char* fullPath = NULL;
  static char* pathPrefix = NULL;
  static char* programName = NULL;
  char* p, tmp;
  ssize_t r;

  if ( fullPath == NULL ) {
    fullPath = malloc(MAX_EXE_PATH_STR_SIZE);
    if ( fullPath == NULL ) {
      fprintf(stderr, "Insufficient memory: %s\n", strerror(errno));
      exit(0);
    }

    r = readlink("/proc/self/exe", fullPath, MAX_EXE_PATH_STR_SIZE);

    if ( r == -1 ) {
      fprintf(stderr, "readlink failed: %s\n", strerror(errno));
      exit(0);
    }

    fullPath[r] = '\0';
    p = strrchr(fullPath, '/'); tmp = *p; *p = '\0';
    pathPrefix = strdup(fullPath);
    programName = p+1;
    *p = tmp;
  }
  if (fpath != NULL) *fpath = fullPath;
  if (ppath != NULL) *ppath = pathPrefix;
  if (pname != NULL) *pname = programName;
}

/*
 * There are two possible places for where the logging daemon will exist.
 * Normally, the logging daemon will be in the ../libexec directory of the
 * place where umap is installed/deployed.  For developers, the other place
 * is in the build directory relative to where running umap program is being
 * run.
 *
 * This function will first attempt to find the executable in the installation
 * location.  If it does not find the file there, it will then check the
 * directory relative to where the program being run was built.
 *
 * If neither are found, this function will print an error and will cause the
 * forked daemon to just exit and no logging will be performed.
 */
void spawnLogDaemon(char *tempdir)
{
    int result = fork();

    if (result == 0) {
        result = fork();
        if (result == 0) {
            char *params[7];
            int cur = 0;
            char* path_prefix;
            char* pname_pri;
            char* pname_alt;
            char* pname;
            struct stat sbuf;

            getProgramAndPath( NULL, &path_prefix, NULL);

            pname_pri = malloc(strlen(path_prefix) + strlen(spindle_log_daemon_name) + 1);
            if ( pname_pri == NULL ) {
                fprintf(stderr, "Insufficient memory: %s\n", strerror(errno));
                exit(0);
            }
            pname_pri[0] = '\0';
            sprintf(pname_pri, "%s/%s", path_prefix, spindle_log_daemon_name);

            pname_alt = malloc(strlen(path_prefix) + strlen(spindle_log_daemon_name2) + 1);
            if ( pname_alt == NULL ) {
                fprintf(stderr, "Insufficient memory: %s\n", strerror(errno));
                exit(0);
            }
            pname_alt[0] = '\0';
            sprintf(pname_alt, "%s/%s", path_prefix, spindle_log_daemon_name2);

            pname = pname_pri;
            if ( stat(pname_pri, &sbuf) < 0 ) {
              if ( stat(pname_alt, &sbuf) == 0 ) {
                pname = pname_alt;
              }
            }

            params[cur++] = pname;
            params[cur++] = tempdir;
            if (spindle_debug_prints) {
                params[cur++] = "-debug";
                params[cur++] = "umap_output";
            }
            params[cur++] = NULL;

            execv(pname, params);
            fprintf(stderr, "Error executing %s: %s\n", pname, strerror(errno));
            exit(0);
        }
        else {
            exit(0);
        }
    }
    else
    {
      int status;
      do {
        waitpid(result, &status, 0);
      } while (!WIFEXITED(status));
    }
}

int clearDaemon(char *tmpdir)
{
   int fd;
   char reset_buffer[512];
   char lock_buffer[512];
   char log_buffer[512];
   int pid;

   /* Only one process can reset the daemon */
   snprintf(reset_buffer, 512, "%s/umap_log_reset", tmpdir);
   fd = open(reset_buffer, O_WRONLY | O_CREAT | O_EXCL, 0600);
   if (fd == -1)
      return 0;
   close(fd);

   snprintf(lock_buffer, 512, "%s/umap_log_lock", tmpdir);
   snprintf(log_buffer, 512, "%s/umap_log", tmpdir);

   fd = open(lock_buffer, O_RDONLY);
   if (fd != -1) {
      char pids[32], *cur = pids;
      while (read(fd, cur++, 1) == 1 && (cur - pids) < 32);
      *cur = '\0';
      pid = atoi(pids);
      if (pid && kill(pid, 0) != -1) {
         /* The process exists, someone else likely re-created it */
         return 0;
      }
   }

   unlink(log_buffer);
   unlink(lock_buffer);
   unlink(reset_buffer);

   return 1;
}

int connectToLogDaemon(char *path)
{
   int result, pathsize, sockfd;
   struct sockaddr_un saddr;

   sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (sockfd == -1)
      return -1;

   bzero(&saddr, sizeof(saddr));
   pathsize = sizeof(saddr.sun_path);
   saddr.sun_family = AF_UNIX;
   strncpy(saddr.sun_path, path, pathsize-1);

   int timeout = 0;
   for (;;) {
      result = connect(sockfd, (struct sockaddr *) &saddr, sizeof(struct sockaddr_un));
      if (result == -1 && (errno == ECONNREFUSED || errno == ENOENT)) {
         timeout++;
         if (timeout == CONNECT_TIMEOUT)
            return -1;
         usleep(100000); /* .1 seconds */
      }
      else if (result == -1) {
         fprintf(stderr, "Error connecting: %s\n", strerror(errno));
         return -1;
      }
      else {
         break;
      }
   }

   return sockfd;
}

static void setConnectionSurvival(int fd, int survive_exec)
{
   if (fd == -1)
      return;

   if (!survive_exec) {
      int fdflags = fcntl(fd, F_GETFD, 0);
      if (fdflags < 0)
         fdflags = 0;
      fcntl(fd, F_SETFD, fdflags | O_CLOEXEC);
      unsetenv("UMAP_LOGGING_SOCKET");
   }
   else {
      int fdflags = fcntl(fd, F_GETFD, 0);
      if (fdflags < 0)
         fdflags = 0;
      fcntl(fd, F_SETFD, fdflags & ~O_CLOEXEC);
      char fd_str[32];
      snprintf(fd_str, 32, "%d", debug_fd);
      setenv("UMAP_LOGGING_SOCKET", fd_str, 1);
   }
}

static int setup_connection(char *connection_name)
{
   char *socket_file;
   int socket_file_len;
   int fd, result;

   socket_file_len = strlen(tempdir) + strlen(connection_name) + 2;
   socket_file = (char *) malloc(socket_file_len);
   snprintf(socket_file, socket_file_len, "%s/%s", tempdir, connection_name);

   int tries = 5;
   for (;;) {
      /* If the daemon doesn't exist, create it and wait for its existance */
      if (!fileExists(socket_file)) {
         spawnLogDaemon(tempdir);

         int timeout = 0;
         while (!fileExists(socket_file) && timeout < SPAWN_TIMEOUT) {
            usleep(100000); /* .1 seconds */
            timeout++;
         }

         if (timeout == SPAWN_TIMEOUT) {
            free(socket_file);
            return -1;
         }
      }

      /* Establish connection to daemon */
      fd = connectToLogDaemon(socket_file);
      if (fd != -1)
         break;

      /* Handle failed connection. */
      if (--tries == 0)
         break;

      result = clearDaemon(tempdir);
      if (!result) {
         /* Give the process clearing the daemon a chance to finish, then
            try again */
         sleep(1);
      }
   }
   free(socket_file);
   return fd;
}

void reset_spindle_debugging()
{
   spindle_debug_prints = 0;
   init_spindle_debugging(0);
}

void init_spindle_debugging(int survive_exec)
{
   char *already_setup, *log_level_str;
   int log_level = 0;

   log_level_str = getenv("UMAP_LOGGING");
   if (log_level_str)
      log_level = atoi(log_level_str);
   spindle_debug_prints = log_level;
   if (!log_level)
      return;

   if (run_local_only) {
     spindle_debug_output_f = stdout;
     return;
   }

   getProgramAndPath( NULL, NULL, &spindle_debug_name);

   if (spindle_debug_prints)
      return;

   /* Setup locations for temp and output files */
   tempdir = getenv("TMPDIR");
   if (!tempdir)
      tempdir = getenv("TEMPDIR");
   if (!tempdir || !*tempdir)
      tempdir = "/tmp";
   if (!fileExists(tempdir)) {
      spindle_mkdir(tempdir);
   }

   debug_location = log_level ? "./umap_output" : NULL;

   already_setup = getenv("UMAP_LOGGING_SOCKET");
   if (already_setup) {
      sscanf(already_setup, "%d", &debug_fd);
   }
   else {
      if (log_level)
         debug_fd = setup_connection("umap_log");
   }

   setConnectionSurvival(debug_fd, survive_exec);

   /* Setup the variables */
   if (debug_fd != -1)
      spindle_debug_output_f = fdopen(debug_fd, "w");
}

void spindle_dump_on_error()
{
   void *stacktrace[256];
   char **syms;
   int size, i;

   size = backtrace(stacktrace, 256);
   if (size <= 0)
      return;
   syms = backtrace_symbols(stacktrace, size);

   for (i = 0; i<size; i++) {
      fprintf(spindle_debug_output_f, "%p - %s\n", stacktrace[i], syms && syms[i] ? syms[i] : "<NO NAME>");
   }

   if (syms)
      free(syms);
}

void fini_spindle_debugging()
{
   static unsigned char exitcode[8] = { 0x01, 0xff, 0x03, 0xdf, 0x05, 0xbf, 0x07, '\n' };
   if (debug_fd != -1)
      write(debug_fd, &exitcode, sizeof(exitcode));
}

int is_debug_fd(int fd)
{
   return (fd == debug_fd);
}
#endif
