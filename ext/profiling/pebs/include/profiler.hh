#ifndef mt_PROFILER_H
#define mt_PROFILER_H

//#include <boost/thread/shared_mutex.hpp>
#include <numa.h>
#include <atomic>
#include "mem_struct.hh"
//#include "perf_wrap.hh"

struct perf_sample
{
    struct perf_event_header header;
    uint64_t time_stamp;
    uint64_t addr;
};

struct memory_map_key
{
    memory_address ma;
    bool is_search=false;
};

struct address_comparator
{
    bool operator() (const memory_map_key &lhs, const memory_map_key &rhs) const
    {
        if (lhs.ma.start_address == rhs.ma.start_address)
            return 0;
        if (lhs.is_search && rhs.is_search)
        {
            throw PerfException("Comparator two search elements error");
        }
        if (lhs.is_search)
            return search_comparator(lhs, rhs);
        if (lhs.ma.start_address > rhs.ma.start_address)
            return 1;
        else
            return 0;
    }

    bool search_comparator(const memory_map_key &lhs, const memory_map_key &rhs) const
    {
        if ((unsigned long) lhs.ma.start_address > (long)rhs.ma.start_address + rhs.ma.offset)
            return 1;
        else
            return 0;
    }
};

class Profiler: public PerfWrap
{
public:
    void mt_register_address (void *, size_t);
    void mt_unregister_address (void *);
    void __register_tier_address (void *, size_t, size_t);
    void __unregister_tier_address (void *, size_t, size_t);

    void reset();
    void view_all(int n = 0);
    void view_topN(int n);

    Profiler();
    ~Profiler();

private:
    map<memory_map_key, metrics, address_comparator> data;
    size_t __align_size (size_t);
    size_t __get_chunk_size (size_t);
    boost::shared_mutex __mutex;
    std::atomic<long> total_bytes;

    virtual const string* __get_events_array() override;
    virtual int __get_events_num() override;
    virtual long __get_sample_type() override;
    virtual long __get_sample_period() override;
    virtual void sample_callback(int, pid_t, void *, long) override;

    void __monitor_hook();
    void __reset_metrics(metrics);

};

#endif
