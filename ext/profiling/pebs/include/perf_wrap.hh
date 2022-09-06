#ifndef PROF_PERFWRAP_H
#define PROF_PERFWRAP_H

#include <perfmon/perf_event.h>
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/epoll.h>
#include <boost/thread/shared_mutex.hpp>
#include <iostream>
#include <string>
#include <map>
#include <thread>
#include <mutex>

#define RING_BUFFER_PAGES 16
#define MAX_EVENTS 1024
#define PAGE_SIZE 4096

using namespace std;

struct fd_buffer
{
    pid_t pid;
    void *buf;
    long next_offset;
};

struct thread_fds
{
    int* sample_fds;
    int* count_fds;
};

struct PerfException: public exception
{
    string m;
    PerfException(string msg)
    {
        m = msg;
    }

    const char* what () const throw ()
    {
        return m.data();
    }
};

class PerfWrap
{
public:
    void register_thread(pid_t);
    void unregister_thread(pid_t);
    bool check_thread(pid_t);
    void start_all();
    void stop_all();
    PerfWrap ();
    virtual ~PerfWrap ();

protected:
    long* get_and_reset_counter();
    long get_and_reset_timer();
    int events_num=-1;
    void enroll();
    void unroll();

private:
    perf_event_attr **attrs;
    int epoll_fd = -1;
    volatile bool is_running = false;
    volatile bool has_init = false;
    volatile bool has_finished=true;
    long __timer = -1;
    map<int, fd_buffer> fd_to_buffer;
    map<pid_t, thread_fds> thread_to_fds;
    vector<pid_t> thread_buffer;
    boost::shared_mutex map_mt_0;
    boost::shared_mutex map_mt_1;
    mutex init_mutex;
    mutex thread_buffer_mutex;

    void __init();
    perf_event_attr __init_perf_attr(string, bool);
    int __perf_open_base(perf_event_attr, pid_t);
    int __perf_open_count(perf_event_attr, pid_t);
    int __perf_open_sample(perf_event_attr, pid_t);
    void  __perf_listener();
    void __start_fd(int);
    void __stop_fd(int);

    virtual const string* __get_events_array()=0;
    virtual int __get_events_num()=0;
    virtual long __get_sample_type()=0;
    virtual long __get_sample_period()=0;
    virtual void sample_callback(int, pid_t, void *, long);
};

#endif
