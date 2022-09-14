
#include <fstream>
#include <iostream>
#include <fcntl.h>

#include "profiler.hh"

Profiler::Profiler()
{
    events_num = __get_events_num();
    total_bytes = 0;
}

void Profiler::mt_register_address (void *p, size_t bytes)
{
    const int init_size = 1048576; //1MB sub-region
    size_t num_subregions = (bytes+init_size-1)/init_size;
    void *addr = p;
    
    for(size_t i = 0; i<num_subregions; i++){
        memory_map_key key;
        memory_address ma;
        ma.start_address = addr;
        addr += init_size;
        ma.offset = init_size - 1;
        key.ma = ma;
        metrics m;
        m.samples = (atomic<long>*) malloc(sizeof(atomic<long>));
        *(m.samples) = 0;
        m.ma = ma;
        m.bytes = init_size;
        total_bytes += m.bytes;
        __mutex.lock();
        if (data.find(key) != data.end())
            throw PerfException("Can't register same address!");
        data[key] = m;
        __mutex.unlock();
    }
}

void Profiler::mt_unregister_address (void *p)
{
    memory_map_key key;
    memory_address ma;
    ma.start_address = p;
    key.ma = ma;
    key.is_search = true;
    __mutex.lock();
    map<memory_map_key, metrics>::iterator it = data.find(key);
    if (it == data.end())
    {
        throw PerfException("memory address doesn't exist!");
    }
    total_bytes -= it->second.bytes;
    free(it->second.samples);
    data.erase(key);
    __mutex.unlock();
}

void Profiler::__register_tier_address (void *p, size_t __size, size_t page_num_per_tier)
{
    int index = 0;
    size_t tier_size = page_num_per_tier * PAGE_SIZE;
    do
    {
        if ((index + 1) * tier_size > __size)
            mt_register_address((void*)((long) p + index * tier_size), __size - index * tier_size);
        else
            mt_register_address((void*)((long) p + index * tier_size), tier_size);
        index++;
    }
    while (index * tier_size < __size);
    //printf("register memory size: %d tier_num: %d\n", __size, index);
}

void Profiler::__unregister_tier_address (void *p, size_t __size, size_t page_num_per_tier)
{
    size_t index = 0;
    size_t tier_size = page_num_per_tier * PAGE_SIZE;
    do
    {
        mt_unregister_address ((void*)((long)p + index * tier_size));
        index++;
    }
    while (index * tier_size < __size);
}

size_t Profiler::__align_size (size_t __size)
{
    size_t page_num = __size % PAGE_SIZE == 0 ? __size / PAGE_SIZE : __size / PAGE_SIZE + 1;
    return page_num * PAGE_SIZE;
}

size_t Profiler::__get_chunk_size(size_t __size)
{
    size_t page_num = __size % PAGE_SIZE == 0 ? __size / PAGE_SIZE : __size / PAGE_SIZE + 1;
    size_t chunk_size = page_num / CHUNK_NUM;
    chunk_size = chunk_size < MIN_CHUNK_SIZE ? MIN_CHUNK_SIZE : chunk_size;
    return chunk_size;
}


//KNL: MEM_UOPS_RETIRED:L2_MISS_LOADS
//SKX: MEM_LOAD_UOPS_RETIRED:L2_MISS
const string* Profiler::__get_events_array()
{
    string* res = new string[1];
    res[0] = "MEM_LOAD_UOPS_RETIRED:L2_MISS";

    return res;
}

int Profiler::__get_events_num()
{
    return 1;
}

long Profiler::__get_sample_type()
{
    return PERF_SAMPLE_ADDR | PERF_SAMPLE_TIME;
}

long Profiler::__get_sample_period()
{
    long bytes = total_bytes.load();
    long num_pages = (long) std::ceil(bytes/16384);
    long res = (long) num_pages/200/64;

    INFO("total number of 16K pages: %lu , sample period: %lu", num_pages, res);

    return res;
}

void Profiler::sample_callback(int fd, pid_t pid, void * buf, long offset)
{
    perf_sample *sample = (perf_sample *) ((uint8_t *) buf + offset);
    if (sample->header.type == PERF_RECORD_SAMPLE)
    {

        memory_map_key search;
        memory_address ma;
        ma.start_address = (void *) sample->addr;
        search.ma = ma;
        search.is_search = true;
        __mutex.lock_shared();
        map<memory_map_key, metrics, address_comparator>::iterator f = data.find(search);
        if (f != data.end())
        {
            (*(f->second.samples))++;
            //long c = (*(it->second.samples));
            //DEBUG("Find memory: pid: %d, addr: %p, %ld bytes, tier %d, samples %ld\n", 
            //                    pid, f->second.ma.start_address, f->second.bytes, f->second.tier_id, c);
        }
        __mutex.unlock_shared();
    }
}

void Profiler::reset()
{
    __mutex.lock_shared();
    auto it = data.begin();
    while (it != data.end())
    {
        __reset_metrics(it->second);
        it++;
    }
    __mutex.unlock_shared();
}

void Profiler::view_all(int n)
{
    std::vector<std::pair<memory_map_key, metrics>> vec;
    std::copy(data.begin(), data.end(), std::back_inserter<std::vector<std::pair<memory_map_key, metrics>>>(vec));
    std::sort(vec.begin(), vec.end(),
                [](const std::pair<memory_map_key, metrics>& l, const std::pair<memory_map_key, metrics>& r) {
                    return *(l.second.samples)> *(r.second.samples);
                });

    auto it = vec.begin();
    int  d = 0;
    int  sample_req = __get_sample_period();
    while (it != vec.end())
    {
        long c = (*(it->second.samples));
        if(c==0) break;
        if(n>0 && d==n) break;
        INFO("Region %d : %p : %ld bytes: sampled access %ld", d++, it->second.ma.start_address, it->second.bytes, c*sample_req);
        it++;
    }
    printf("\n");

}

void Profiler::view_topN(int n)
{
    view_all(n);

}

void Profiler::__reset_metrics(metrics m)
{
    *(m.samples) = 0;
}

Profiler::~Profiler()
{
    data.clear();
}
