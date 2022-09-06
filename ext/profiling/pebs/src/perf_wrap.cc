#include "perf_wrap.hh"

PerfWrap::PerfWrap(): epoll_fd(epoll_create(1024))
{
    if (epoll_fd < 0)
        throw PerfException("epoll error!");
    if (PFM_SUCCESS != pfm_initialize())
        throw PerfException("PFM init fail!");
}

void PerfWrap::register_thread(pid_t pid)
{
    map_mt_0.lock_shared();
    if (thread_to_fds.find(pid) != thread_to_fds.end())
    {
        printf("pid:%d has been registerd!", pid);
        map_mt_0.unlock_shared();
        return;
    }
    map_mt_0.unlock_shared();
    __init();
    thread_fds fds;
    fds.count_fds = new int[__get_events_num()];
    fds.sample_fds = new int[__get_events_num()];
    for (int i=0; i<__get_events_num(); i++)
        fds.count_fds[i] = __perf_open_count(attrs[0][i], pid);
    for (int i=0; i<__get_events_num(); i++)
        fds.sample_fds[i] = __perf_open_sample(attrs[1][i], pid);
    map_mt_0.lock();
    thread_to_fds[pid] = fds;
    map_mt_0.unlock();
    if (is_running)
    {
        for (int i=0; i<__get_events_num(); i++)
        {
            __start_fd(fds.count_fds[i]);
            __start_fd(fds.sample_fds[i]);
        }
    }
}

void PerfWrap::unregister_thread(pid_t pid)
{
    map_mt_0.lock_shared();
    map<pid_t, thread_fds>::iterator it = thread_to_fds.find(pid);
    if (it == thread_to_fds.end())
    {
        map_mt_0.unlock_shared();
        return;
    }
    map_mt_0.unlock_shared();
    if (is_running)
    {
        for (int i=0; i<__get_events_num(); i++)
        {
            __stop_fd(it->second.count_fds[i]);
            __stop_fd(it->second.sample_fds[i]);
            close(it->second.count_fds[i]);
            close(it->second.sample_fds[i]);
            map_mt_1.lock_shared();
            munmap (fd_to_buffer[it->second.sample_fds[i]].buf, (1 + RING_BUFFER_PAGES) * PAGE_SIZE);
            map_mt_1.unlock_shared();
            map_mt_1.lock();
            fd_to_buffer.erase(it->second.sample_fds[i]);
            map_mt_1.unlock();
        }
    }
    delete[] it->second.count_fds;
    delete[] it->second.sample_fds;
    map_mt_0.lock();
    thread_to_fds.erase(pid);
    map_mt_0.unlock();
}

bool PerfWrap::check_thread(pid_t pid)
{
    bool res;
    map_mt_0.lock_shared();
    res = thread_to_fds.find(pid) == thread_to_fds.end() ? false : true;
    map_mt_0.unlock_shared();
    return res;
}

void PerfWrap::enroll()
{
    thread_buffer_mutex.lock();
    thread_buffer.push_back((pid_t)syscall(__NR_gettid));
    thread_buffer_mutex.unlock();
}

void PerfWrap::unroll()
{
    unregister_thread((pid_t)syscall(__NR_gettid));
}

void PerfWrap::start_all()
{
    enroll();

    is_running = true;
    thread_buffer_mutex.lock();
    for (size_t i = 0; i < thread_buffer.size(); i++)
        register_thread(thread_buffer[i]);
    thread_buffer.clear();
    thread_buffer_mutex.unlock();
    map_mt_0.lock_shared();
    map<pid_t, thread_fds>::iterator iter = thread_to_fds.begin();
    while (iter != thread_to_fds.end())
    {
        for (int i = 0; i < __get_events_num(); i++)
        {
            __start_fd(iter->second.count_fds[i]);
            __start_fd(iter->second.sample_fds[i]);
        }
        iter++;
    }
    map_mt_0.unlock_shared();
    __timer = clock();
    has_finished=false;
    thread t(&PerfWrap::__perf_listener, this);
    t.detach();
}

void PerfWrap::stop_all()
{

    map_mt_0.lock_shared();
    map<pid_t, thread_fds>::iterator iter = thread_to_fds.begin();
    while (iter != thread_to_fds.end())
    {
        for (int i = 0; i < events_num; i++)
        {
            __stop_fd(iter->second.count_fds[i]);
            __stop_fd(iter->second.sample_fds[i]);
        }
        iter++;
    }
    is_running = false;
    map_mt_0.unlock_shared();

    unroll();
}

long* PerfWrap::get_and_reset_counter()
{
    long* res = new long[__get_events_num()];
    for (int i = 0; i < __get_events_num(); i++)
        res[i] = 0;
    map_mt_1.lock_shared();
    map<pid_t, thread_fds>::iterator it = thread_to_fds.begin();
    while (it != thread_to_fds.end())
    {
        uint64_t counter;
        for (int i = 0; i < __get_events_num(); i++)
        {
            read(it->second.count_fds[i], &counter, sizeof(counter));
            ioctl(it->second.count_fds[i], PERF_EVENT_IOC_RESET, 0);
            res[i] += counter;
        }
        it++;
    }
    map_mt_1.unlock_shared();
    return res;
}

long PerfWrap::get_and_reset_timer()
{
    long res = clock() - __timer;
    __timer = clock();
    return res;
}

void PerfWrap::__init()
{
    if (!has_init)
    {
        init_mutex.lock();
        if (!has_init)
        {
            attrs = new perf_event_attr*[2];
            const string* events_array = __get_events_array();
            for (int i = 0; i < 2; i++)
            {
                attrs[i] = new perf_event_attr[__get_events_num()];
                for (int k = 0; k < __get_events_num(); k++)
                    attrs[i][k] = __init_perf_attr(events_array[k], i);
            }
            delete[] events_array;
            has_init = true;
        }
        init_mutex.unlock();
    }
}

void PerfWrap::__start_fd(int fd)
{
    ioctl (fd, PERF_EVENT_IOC_RESET, 0);
    ioctl (fd, PERF_EVENT_IOC_ENABLE, 0);
}

void PerfWrap::__stop_fd(int fd)
{
    ioctl (fd, PERF_EVENT_IOC_DISABLE, 0);
}

int PerfWrap::__perf_open_sample(perf_event_attr attr, pid_t pid)
{
    int fd = __perf_open_base(attr, pid);
    if (fd < 0)
        throw PerfException("Perf sample open error");
    fd_buffer buffer;
    buffer.buf = mmap (0, (1 + RING_BUFFER_PAGES) * PAGE_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if ((long) buffer.buf < 0)
        throw PerfException("Cannot mmap!");
    buffer.pid = pid;
    buffer.next_offset = 0;
    map_mt_1.lock();
    fd_to_buffer[fd] = buffer;
    map_mt_1.unlock();
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
        throw PerfException("Epoll error!");
    return fd;
}

int PerfWrap::__perf_open_count(perf_event_attr attr, pid_t pid)
{
    int fd = __perf_open_base(attr, pid);
    if (fd < 0)
        throw PerfException("Perf count open error");
    return fd;
}

int PerfWrap::__perf_open_base(perf_event_attr attr, pid_t pid)
{
    return perf_event_open(&attr, pid, -1, -1, 0);
}

perf_event_attr PerfWrap::__init_perf_attr(string event_name, bool is_sample)
{
    perf_event_attr attr;
    memset(&attr, 0, sizeof(perf_event_attr));
    attr.size = sizeof(perf_event_attr);
    pfm_perf_encode_arg_t arg;
    memset (&arg, 0, sizeof (arg));
    arg.size = sizeof (pfm_perf_encode_arg_t);
    arg.attr = &attr;
    char *fstr;
    arg.fstr = &fstr;
    if (PFM_SUCCESS != pfm_get_os_event_encoding (event_name.data(),
            PFM_PLM0 | PFM_PLM3, PFM_OS_PERF_EVENT, &arg))
    {
        stringstream ss;
        ss << "pfm_get_os_event_encoding "<< event_name.data()<<" fail";
        throw PerfException(ss.str());
    }
    else
    {
        printf("pfm_get_os_event_encoding %s successfully \n", event_name.data());
    }

    if (is_sample)
    {
        attr.sample_period = __get_sample_period();
        attr.sample_type = __get_sample_type();
        attr.mmap = 1;
        attr.precise_ip = 2;
        attr.wakeup_events=1;
    }
    attr.task = 1;
    attr.use_clockid = 1;
    attr.clockid = CLOCK_MONOTONIC_RAW;
    // Other parameters
    attr.disabled = 1;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    return attr;
}

void PerfWrap::sample_callback(int fd, pid_t pid, void *buf, long offset)
{
    //donothing
}

void PerfWrap::__perf_listener()
{
    struct epoll_event events[MAX_EVENTS];
    while (is_running)
    {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
        if (nfds < 0)
        {
            printf("Epoll wait error%d\n", nfds);
            continue;
        }
        for (int k = 0; k < nfds; k++)
        {
            int event_fd = events[k].data.fd;
            map_mt_1.lock_shared();
            map<int, fd_buffer>::iterator it = fd_to_buffer.find(event_fd);
            if (it == fd_to_buffer.end())
            {
                printf("fd doesn't register?%d\n", event_fd);
                map_mt_1.unlock_shared();
                continue;
            }
            map_mt_1.unlock_shared();
            sample_callback(event_fd, it->second.pid, it->second.buf, PAGE_SIZE + it->second.next_offset);
            perf_event_mmap_page *rinfo = (perf_event_mmap_page*) it->second.buf;
            it->second.next_offset = rinfo->data_head % (RING_BUFFER_PAGES * PAGE_SIZE);
        }
    }
    has_finished = true;
}

PerfWrap::~PerfWrap()
{
    stop_all();
    while (!has_finished)
        sleep(1);
    map<int, thread_fds>::iterator fd_iter = thread_to_fds.begin();
    while (fd_iter != thread_to_fds.end())
    {
        for (int i=0; i<events_num; i++)
        {
            close(fd_iter->second.count_fds[i]);
            close(fd_iter->second.sample_fds[i]);
            munmap (fd_to_buffer[fd_iter->second.sample_fds[i]].buf, (1 + RING_BUFFER_PAGES) * PAGE_SIZE);
            fd_to_buffer.erase(fd_iter->second.sample_fds[i]);
        }
        delete[] fd_iter->second.count_fds;
        delete[] fd_iter->second.sample_fds;
        fd_iter++;
    }
    thread_to_fds.clear();
    if (has_init)
    {
        for (int i = 0; i < 2; i++)
        {
            delete[] attrs[i];
        }
        delete[] attrs;
    }
}
