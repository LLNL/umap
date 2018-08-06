/*
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

#include "spindle_logc.h"
#include "config.h"

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

#if !defined(LIBEXEC)
#error Expected to have LIBEXEC defined
#endif
#if !defined(DAEMON_NAME)
#error Expected to have DAEMON_NAME defined
#endif

static char spindle_log_daemon_name[] = LIBEXEC "/" DAEMON_NAME;

static int debug_fd = -1;
static int test_fd = -1;
static char *tempdir;
static char *debug_location;
static char *test_location;

FILE *spindle_test_output_f;
FILE *spindle_debug_output_f;
char *spindle_debug_name = "UNKNOWN";
int spindle_debug_prints;
int run_tests;

//Timeout in tenths of a second
#define SPAWN_TIMEOUT 300
#define CONNECT_TIMEOUT 100

extern int spindle_mkdir(char *orig_path);

int fileExists(char *name) 
{
   struct stat buf;
   return (stat(name, &buf) != -1);
}

void spawnLogDaemon(char *tempdir)
{
#if !defined(SPINDLECLIENT) || !defined(os_bluegene)
   int result = fork();
   if (result == 0) {
      result = fork();
      if (result == 0) {
         char *params[7];
         int cur = 0;
         params[cur++] = spindle_log_daemon_name;
         params[cur++] = tempdir;
         if (spindle_debug_prints) {
            params[cur++] = "-debug";
            params[cur++] = "spindle_output";
         }
         if (run_tests) {
            params[cur++] = "-test";
            params[cur++] = "spindle_test";
         }
         params[cur++] = NULL;
         
         execv(spindle_log_daemon_name, params);
         fprintf(stderr, "Error executing %s: %s\n", spindle_log_daemon_name, strerror(errno));
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
#endif
}

int clearDaemon(char *tmpdir)
{
   int fd;
   char reset_buffer[512];
   char lock_buffer[512];
   char log_buffer[512];
   char test_buffer[512];
   int pid;

   /* Only one process can reset the daemon */
   snprintf(reset_buffer, 512, "%s/spindle_log_reset", tmpdir);
   fd = open(reset_buffer, O_WRONLY | O_CREAT | O_EXCL, 0600);
   if (fd == -1)
      return 0;
   close(fd);

   snprintf(lock_buffer, 512, "%s/spindle_log_lock", tmpdir);
   snprintf(log_buffer, 512, "%s/spindle_log", tmpdir);
   snprintf(test_buffer, 512, "%s/spindle_test", tmpdir);

   fd = open(lock_buffer, O_RDONLY);
   if (fd != -1) {
      char pids[32], *cur = pids;
      while (read(fd, cur++, 1) == 1 && (cur - pids) < 32);
      cur = '\0';
      pid = atoi(pids);
      if (pid && kill(pid, 0) != -1) {
         /* The process exists, someone else likely re-created it */
         return 0;
      }
   }

   unlink(log_buffer);
   unlink(test_buffer);
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
      unsetenv("SPINDLE_DEBUG_SOCKET");
   }
   else {
      int fdflags = fcntl(fd, F_GETFD, 0);
      if (fdflags < 0)
         fdflags = 0;
      fcntl(fd, F_SETFD, fdflags & ~O_CLOEXEC);
      char fd_str[32];
      snprintf(fd_str, 32, "%d %d", debug_fd, test_fd);
      setenv("SPINDLE_DEBUG_SOCKET", fd_str, 1);
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
   run_tests = 0;
   init_spindle_debugging(spindle_debug_name, 0);
}

void init_spindle_debugging(char *name, int survive_exec)
{
   char *already_setup, *log_level_str;
   int log_level = 0;

   spindle_debug_name = name;

   if (spindle_debug_prints || run_tests)
      return;

   run_tests = (getenv("SPINDLE_TEST") != NULL);

   log_level_str = getenv("SPINDLE_DEBUG");
   if (log_level_str)
      log_level = atoi(log_level_str);
   spindle_debug_prints = log_level;
   if (!log_level && !run_tests)
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

   debug_location = log_level ? "./spindle_output" : NULL;
   test_location  = run_tests ? "./spindle_test" : NULL;

   already_setup = getenv("SPINDLE_DEBUG_SOCKET");
   if (already_setup) {
      sscanf(already_setup, "%d %d", &debug_fd, &test_fd);
   }
   else {
      if (log_level)
         debug_fd = setup_connection("spindle_log");
      if (run_tests)
         test_fd = setup_connection("spindle_test");
   }

   setConnectionSurvival(debug_fd, survive_exec);
   setConnectionSurvival(test_fd, survive_exec);

   /* Setup the variables */
   if (debug_fd != -1)
      spindle_debug_output_f = fdopen(debug_fd, "w");
   if (test_fd != -1)
      spindle_test_output_f = fdopen(test_fd, "w");      
}

void spindle_dump_on_error()
{
   void *stacktrace[256];
   char **syms;
   int size, i;

   if (strstr(spindle_debug_name, "Client")) {
      return;
   }

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
   if (test_fd != -1)
      write(test_fd, &exitcode, sizeof(exitcode));
}

int is_debug_fd(int fd)
{
   return (fd == debug_fd || fd == test_fd);
}
