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

#include <string>
#include <vector>
#include <set>
#include <map>
#include <cstring>
#include <cassert>

#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>

using namespace std;

//Seconds to live without a child
#define TIMEOUT 10

string tmpdir;
string debug_fname;
string test_fname;


void clean();
void cleanFiles();

class UniqueProcess;
class OutputLog;
class MsgReader;
class TestLog;

UniqueProcess *lockProcess;
OutputLog *debug_log;
MsgReader *debug_reader;
TestLog *test_log;
MsgReader *test_reader;

bool runTests = false;
bool runDebug = false;

static unsigned char exitcode[8] = { 0x01, 0xff, 0x03, 0xdf, 0x05, 0xbf, 0x07, '\n' };

class UniqueProcess
{
private:
   int fd;
   string logFileLock;
   bool unique;
public:
   UniqueProcess()
   {
      unique = false;
      logFileLock = tmpdir + string("/spindle_log_lock");
      fd = open(logFileLock.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
      if (fd != -1) {
         char pid_str[32];
         snprintf(pid_str, 32, "%d", getpid());
         write(fd, pid_str, strlen(pid_str));
         unique = true;
         return;
      }
      if (errno == EEXIST)
         return;
      fprintf(stderr, "Error creating lock file %s: %s\n", logFileLock.c_str(), strerror(errno));
   }

   ~UniqueProcess()
   {
      if (fd < 0)
         return;
      close(fd);
      unlink(logFileLock.c_str());
   }

   void cleanFile() {
      if (fd < 0)
         return;
      close(fd);
      unlink(logFileLock.c_str());
      fd = -1;
   }

   bool isUnique() const 
   {
      return unique;
   }
};

class OutputInterface
{
public:
   OutputInterface()
   {
   }

   virtual ~OutputInterface()
   {
   }

   bool isExitCode(const char *msg1, int msg1_size, const char *msg2, int msg2_size)
   {
      if (msg1[0] != (char) exitcode[0])
         return false;
      if (msg1_size + msg2_size != 8)
         return false;
      
      char code[8];
      memset(code, 0, sizeof(code));
      int i=0;
      for (i=0; i<msg1_size; i++) 
         code[i] = msg1[i];
      for (int j=0; j<msg2_size; i++, j++)
         code[i] = msg2[j];
      
      for (i = 0; i<8; i++) {
         if (code[i] != (char) exitcode[i]) 
            return false;
      }
      return true;
   }

   virtual void writeMessage(int proc, const char *msg1, int msg1_size, const char *msg2, int msg2_size) = 0;
};

class OutputLog : public OutputInterface
{
   int fd;
   string output_file;
public:
   OutputLog(string fname) :
      output_file(fname)
   {
      char hostname[1024];
      char pid[16];
      int result = gethostname(hostname, 1024);
      hostname[1023] = '\0';
      if (result != -1) {
         output_file += string(".");
         output_file += string(hostname);
      }
      snprintf(pid, 16, "%d", getpid());
      output_file += string(".");
      output_file += string(pid);
      
      fd = creat(output_file.c_str(), 0660);
      if (fd == -1) {
         fprintf(stderr, "[%s:%u] - Error opening output file %s: %s\n",
                 __FILE__, __LINE__, output_file.c_str(), strerror(errno));
         fd = 2; //stderr
      }
   }

   virtual ~OutputLog()
   {
      if (fd != -1 && fd != 2)
         close(fd);
   }

   virtual void writeMessage(int proc, const char *msg1, int msg1_size, const char *msg2, int msg2_size)
   {
      if (isExitCode(msg1, msg1_size, msg2, msg2_size)) {
         /* We've received the exitcode */
         cleanFiles();
         return;
      }

      write(fd, msg1, msg1_size);
      if (msg2)
         write(fd, msg2, msg2_size);
   }
};

class TestVerifier
{
private:
   vector<string> err_strings;
   set<pair<int, string> > target_libs;
   set<pair<int, string> > libs_loaded;
   string tmp_dir;

   void logerror(string s)
   {
      if (debug_log)
         debug_log->writeMessage(0, s.c_str(), s.size(), "\n", 1);
      err_strings.push_back(s);
   }
   
   void checkLoadedVsTarget() {
      set<pair<int, string> >::iterator i, j;
      for (i = target_libs.begin(); i != target_libs.end(); i++) {
         const string &target = i->second;
         if (strstr(target.c_str(), "libnoexist.so") != NULL)
            continue;
         bool found = false;
         for (j = libs_loaded.begin(); j != libs_loaded.end(); j++) {
            if (i->first != j->first)
               continue;
            const string &loaded = j->second;
            if (strstr(loaded.c_str(), target.c_str()) != NULL) {
               found = true;
               break;
            }
         }
         if (!found) {
            char proc_s[16];
            snprintf(proc_s, 16, "%d", i->first);
            logerror(string("Error: Didn't load target: ") + i->second + string(" on proc ") + string(proc_s));
         }
      }
   }

public:
   TestVerifier()
   {
      const char *tmp_s = getenv("TMPDIR");
      if (!tmp_s)
         tmp_s = getenv("TEMPDIR");
      if (!tmp_s)
         tmp_s = getenv("/tmp");
      tmp_dir = tmp_s;
   }

   ~TestVerifier()
   {
   }

   bool parseOpenNotice(int proc, char *filename)
   {
      target_libs.insert(make_pair(proc, string(filename))).second;
      return true;
   }

   bool parseOpen(int proc, char *filename, int ret_code)
   {
      if (strstr(filename, ".so") == NULL &&
          strstr(filename, "retzero") == NULL &&
          strstr(filename, ".py") == NULL)
         return true;
      bool is_from_temp = (strstr(filename, tmp_dir.c_str()) != NULL);

      if (is_from_temp && ret_code == -1) {
         string msg = string("Error: Failed to load from ramdisk: ") + string(filename);
         logerror(msg);
         return false;
      }
      
      if (!is_from_temp && ret_code != -1 && strstr(filename, "libc.so") == NULL) {
         string msg = string("Error: Read shared object from non-ramdisk: ") + string(filename);
         logerror(msg);
         return false;
      }

      if (ret_code != -1 || strstr(filename, "retzero_x")) {
         libs_loaded.insert(make_pair(proc, string(filename)));
      }

      return true;
   }

   bool parseLine(int proc, const char *s) {
      char buffer[4096];
      int ret;

      if (strstr(s, "open(") == s) {
         const char *first_quote, *last_quote, *equals;
         int len;

         first_quote = strchr(s, '"');
         last_quote = strrchr(s, '"');
         if (!first_quote || !last_quote || first_quote == last_quote) {assert(0); return false; }
         len = last_quote - first_quote;
         if (len > 4096) len = 4096;
         strncpy(buffer, first_quote+1, len-1);
         buffer[len-1] = '\0';

         equals = strrchr(s, '=');
         if (!equals || equals[1] != ' ') {assert(0); return false; }
         ret = atoi(equals+2);
         
         parseOpen(proc, buffer, ret);
         if (debug_log)
            debug_log->writeMessage(proc, s, strlen(s), NULL, 0);
         return true;
      }
      const char *spindle_open = strstr(s, "dlstart");
      if (spindle_open) {
         sscanf(spindle_open, "dlstart %s\n", buffer);
         if (debug_log)
            debug_log->writeMessage(proc, s, strlen(s), NULL, 0);
         return parseOpenNotice(proc, buffer);
      }
      if (strcmp(s, "done\n") == 0) {
         checkLoadedVsTarget();
         string fname;
         if (err_strings.empty())
            fname = tmpdir + string("/spindle_test_passed");
         else
            fname = tmpdir + string("/spindle_test_failed");
         int result = creat(fname.c_str(), 0600);
         if (result == -1) {
            fprintf(stderr, "Error created test result file %s: %s\n", fname.c_str(), strerror(errno));
            return false;
         }
         write(result, "0", 1);
         close(result);
         return true;
      }

      if (debug_log)
         debug_log->writeMessage(0, s, strlen(s), NULL, 0);
      return true;
   }
};

class TestLog : public OutputInterface
{
private:
   TestVerifier *test_verifier;   
public:
   TestLog()
   {
      test_verifier = new TestVerifier();
   }

   virtual ~TestLog()
   {
      delete test_verifier;
   }

   virtual void writeMessage(int proc, const char *msg1, int msg1_size, const char *msg2, int msg2_size)
   {
      if (isExitCode(msg1, msg1_size, msg2, msg2_size)) {
         /* We've received the exitcode */
         cleanFiles();
         return;
      }
      
      string s(msg1, msg1_size);
      if (msg2)
         s += string(msg2, msg2_size);
      test_verifier->parseLine(proc, s.c_str());
   }
};

class MsgReader
{
private:
   static const unsigned int MAX_MESSAGE = 4096;
   static const unsigned int LISTEN_BACKLOG = 64;

   struct Connection {
      int fd;
      struct sockaddr_un remote_addr;
      bool shutdown;
      char unfinished_msg[MAX_MESSAGE];
   };

   int sockfd;
   map<int, Connection *> conns;
   char recv_buffer[MAX_MESSAGE];
   size_t recv_buffer_size, named_buffer_size;
   bool error;
   string socket_path;
   pthread_t thrd;
   OutputInterface *log;

   bool addNewConnection() {
      Connection *con = new Connection();
      socklen_t remote_addr_size = sizeof(struct sockaddr_un);
      con->fd = accept(sockfd, (struct sockaddr *) &con->remote_addr, &remote_addr_size);
      con->shutdown = false;
      if (con->fd == -1) {
         fprintf(stderr, "[%s:%u] - Error adding connection: %s\n", __FILE__, __LINE__, strerror(errno));
         delete con;
         return false;
      }

      int flags = fcntl(con->fd, F_GETFL, 0);
      if (flags == -1) flags = 0;
      fcntl(con->fd, F_SETFL, flags | O_NONBLOCK);

      con->unfinished_msg[0] = '\0';
      conns.insert(make_pair(con->fd, con));
      return true;
   }
   
   bool waitAndHandleMessage() {
      fd_set rset;

      for (;;) {
         FD_ZERO(&rset);
         int max_fd = 0;
         if (sockfd != -1) {
            FD_SET(sockfd, &rset);
            max_fd = sockfd;
         }
         
         for (map<int, Connection *>::iterator i = conns.begin(); i != conns.end(); i++) {
            int fd = i->first;
            FD_SET(fd, &rset);
            if (fd > max_fd)
               max_fd = fd;
         }
         
         struct timeval timeout;
         timeout.tv_sec = TIMEOUT;
         timeout.tv_usec = 0;

         if (!max_fd) {
            return false;
         }

         int result = select(max_fd+1, &rset, NULL, NULL, conns.empty() ? &timeout : NULL);
         if (result == 0) {
            return NULL;
         }
         if (result == -1) {
            fprintf(stderr, "[%s:%u] - Error calling select: %s\n", __FILE__, __LINE__, strerror(errno));
            return NULL;
         }

         if (sockfd != -1 && FD_ISSET(sockfd, &rset)) {
            addNewConnection();
         }

         for (map<int, Connection *>::iterator i = conns.begin(); i != conns.end(); i++) {
            int fd = i->first;
            if (FD_ISSET(fd, &rset)) {
               readMessage(i->second);
            }
         }
         
         bool foundShutdownProc;
         do {
            foundShutdownProc = false;
            for (map<int, Connection *>::iterator i = conns.begin(); i != conns.end(); i++) {
               if (i->second->shutdown) {
                  conns.erase(i);
                  foundShutdownProc = true;
                  break;
               }
            }
         } while (foundShutdownProc);
      }
   }

   bool readMessage(Connection *con)
   {
      int result = recv(con->fd, recv_buffer, MAX_MESSAGE, 0);
      if (result == -1) {
         fprintf(stderr, "[%s:%u] - Error calling recv: %s\n", __FILE__, __LINE__, strerror(errno));
         close(con->fd);
         return false;
      }

      if (result == 0) {
         //A client shutdown
         map<int, Connection *>::iterator i = conns.find(con->fd);
         assert(i != conns.end());
         i->second->shutdown = true;
         if (con->unfinished_msg[0] != '\0')
            processMessage(con, "\n", 1);
         close(con->fd);
         return true;
      }

      return processMessage(con, recv_buffer, result);
   }

   bool processMessage(Connection *con, const char *msg, int msg_size) {
      int msg_begin = 0;
      for (int i = 0; i < msg_size; i++) {
         if (msg[i] != '\n')
            continue;

         if (con->unfinished_msg[0] != '\0') {
            log->writeMessage(con->fd, con->unfinished_msg, strlen(con->unfinished_msg),
                              msg + msg_begin, i+1 - msg_begin);
         }
         else {
            log->writeMessage(con->fd, msg + msg_begin, i+1 - msg_begin,
                              NULL, 0);
         }
         con->unfinished_msg[0] = '\0';
         msg_begin = i+1;
      }

      if (msg_begin != msg_size) {
         int remaining_bytes = msg_size - msg_begin;
         strncat(con->unfinished_msg, msg + msg_begin, remaining_bytes);
      }

      return true;
   }

   static void *main_wrapper(void *mreader)
   {
      return static_cast<MsgReader *>(mreader)->main_loop();
   }

   void *main_loop()
   {
      while (waitAndHandleMessage());
      return NULL;
   }

public:
   
   MsgReader(string socket_suffix, OutputInterface *log_) :
      log(log_)
   {
      error = true;

      sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
      if (sockfd == -1) {
         fprintf(stderr, "[%s:%u] - Error calling socket: %s\n", __FILE__, __LINE__, strerror(errno));
         return;
      }

      struct sockaddr_un saddr;
      bzero(&saddr, sizeof(saddr));
      int pathsize = sizeof(saddr.sun_path);
      socket_path = tmpdir + string("/spindle_") + socket_suffix;
      saddr.sun_family = AF_UNIX;
      if (socket_path.length() > (unsigned) pathsize-1) {
         fprintf(stderr, "[%s:%u] - Socket path overflows AF_UNIX size (%d): %s\n",
                 __FILE__, __LINE__, pathsize, socket_path.c_str());
         return;
      }
      strncpy(saddr.sun_path, socket_path.c_str(), pathsize-1);

      int result = bind(sockfd, (struct sockaddr *) &saddr, sizeof(saddr));
      if (result == -1) {
         fprintf(stderr, "[%s:%u] - Error binding socket: %s\n",
                 __FILE__, __LINE__, strerror(errno));
         return;
      }

      result = listen(sockfd, LISTEN_BACKLOG);
      if (result == -1) {
         fprintf(stderr, "[%s:%u] - Error listening socket: %s\n",
                 __FILE__, __LINE__, strerror(errno));
         return;
      }

      error = false;
   }

   ~MsgReader()
   {
      for (map<int, Connection *>::iterator i = conns.begin(); i != conns.end(); i++) {
         int fd = i->first;
         close(fd);
      }
      conns.clear();
      if (sockfd != -1) {
         close(sockfd);
         unlink(socket_path.c_str());
      }
   }

   void cleanFile() {
      close(sockfd);
      unlink(socket_path.c_str());
      sockfd = -1;
   }

   bool hadError() const {
      return error;
   }

   void *run()
   {
      int result = pthread_create(&thrd, NULL, main_wrapper, (void *) this);
      if (result < 0) {
         fprintf(stderr, "Failed to spawn thread: %s\n", strerror(errno));
         return NULL;
      }
      return NULL;
   }

   void join()
   {
      void *result;
      pthread_join(thrd, &result);
   }
};

void parseArgs(int argc, char *argv[])
{
   if (argc < 3) {
      fprintf(stderr, "spindle_logd cannot be directly invoked\n");
      exit(-1);
   }

   tmpdir = argv[1];
   for (int i=0; i<argc; i++) {
      if (strcmp(argv[i], "-debug") == 0) {
         i++;
         debug_fname = argv[i];
         runDebug = true;
      }
      else if (strcmp(argv[i], "-test") == 0) {
         i++;
         test_fname = argv[i];
         runTests = true;
      }
   }
}

void clean()
{
   if (lockProcess)
      delete lockProcess;
   lockProcess = NULL;
   if (debug_log)
      delete debug_log;
   debug_log = NULL;
   if (test_log)
      delete test_log;
   test_log = NULL;
   if (debug_reader)
      delete debug_reader;
   debug_reader = NULL;
}

void cleanFiles()
{
   if (lockProcess)
      lockProcess->cleanFile();
   if (debug_reader)
      debug_reader->cleanFile();
   if (test_reader)
      test_reader->cleanFile();
}

void on_sig(int)
{
   clean();
   exit(0);
}

void registerCrashHandlers()
{
   signal(SIGINT, on_sig);
   signal(SIGTERM, on_sig);
}

int main(int argc, char *argv[])
{
   registerCrashHandlers();
   parseArgs(argc, argv);

   lockProcess = new UniqueProcess();
   if (!lockProcess->isUnique())
      return 0;

   //When running a spindle session we need all stdout closed
   // or a backtick'd `spindle --start-session` may not return.
   // since the output daemon could have forked from the spindle
   // session we may have its pipe from the backticks open.  
   close(0);
   open("/dev/null", O_RDONLY);
   close(1);
   open("/dev/null", O_WRONLY);

   if (runDebug) {
      debug_log = new OutputLog(debug_fname);
      debug_reader = new MsgReader("log", debug_log);
      if (debug_reader->hadError()) {
         fprintf(stderr, "Debug reader error termination\n");
         return -1;
      }
   }
   if (runTests) {
      test_log = new TestLog();
      test_reader = new MsgReader(string("test"), test_log);
      if (test_reader->hadError()) {
         fprintf(stderr, "Test reader error termination\n");
         return -1;
      }      
   }

   if (runDebug)
      debug_reader->run();
   if (runTests)
      test_reader->run();

   if (runDebug)
      debug_reader->join();
   if (runTests)
      test_reader->join();

   clean();
   return 0;
}
