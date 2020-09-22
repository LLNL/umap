//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

/*
   The idea is that we have a single "Load Page" and a set
   of N "Churn Pages" as follows:
  
   +==================================================+
   |                                                  |
   |                Read Load Page(s)                 |
   |                                                  |
   +--------------------------------------------------+
   |                                                  |
   |           Write{/Read} Load Page(s)              |
   |                                                  |
   +--------------------------------------------------+
   |                                                  |
   |               Churn Page #1                      |
   |                                                  |
   +--------------------------------------------------+
   |                                                  |
   |               Churn Page #...                    |
   |                                                  |
   +--------------------------------------------------+
   |                                                  |
   |               Churn Page #N                      |
   |                                                  |
   +==================================================+
  
   We then have a smaller page_buffer_size that these pages will be faulted
   into and madvised out of via umap().
  
   The LoadPage will have a set of num_load_reader and num_load_writer threads
   focussed exclusively on making reads and writes to locations constrained to
   the Load Page.

   The the Churn Pages will have num_churn_reader threads performing random
   byte read accesses across all of the Churn Pages effectively causing the
   Load Page to be paged in and out of the smaller Page_Buffer.
*/
#include <iostream>
#include <cassert>
#include <cstdint>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include <random>
#include <algorithm>
#include <utmpx.h>  // sched_getcpu()
#include <omp.h>

#include "umap/umap.h"
#include "options.h"
#include "../utility/commandline.hpp"
#include "../utility/umap_file.hpp"

uint64_t g_count = 0;
using namespace std;
using namespace chrono;

class pageiotest {
public:
    pageiotest(int _ac, char** _av): time_to_stop{false}, pagesize{utility::umt_getpagesize()} {
        getoptions(options, _ac, _av);

        umt_options.usemmap = options.usemmap;
        umt_options.filename = options.fn;
        umt_options.noinit = options.noinit;
        umt_options.initonly = options.initonly;

        num_rw_load_pages = num_read_load_pages = options.num_load_pages;
        num_churn_pages = options.num_churn_pages;

        base_addr = utility::map_in_file(options.fn, options.initonly,
            options.noinit, options.usemmap,
            (num_churn_pages + num_rw_load_pages + num_read_load_pages) * pagesize);

        if ( base_addr == nullptr ) {
          exit(1);
        }

        assert(base_addr != NULL);

        read_load_pages = base_addr;
        rw_load_pages = (void*)((uint64_t)base_addr + (num_read_load_pages*pagesize));
        churn_pages = (void*)((uint64_t)base_addr + ( (num_read_load_pages + num_rw_load_pages) * pagesize));

        if ( ! options.noinit ) {
            init();
        }

        cout << "Churn Test:\n\t"
            << options.page_buffer_size << " Pages in page buffer\n\t"
            << num_read_load_pages << " Read Load (focus) pages from " << hex << read_load_pages << " - " << (void*)((char*)read_load_pages+((num_read_load_pages*pagesize)-1)) << dec << "\n\t"
            << num_rw_load_pages << " RW Load (focus) pages from " << hex << rw_load_pages << " - " << (void*)((char*)rw_load_pages+((num_rw_load_pages*pagesize)-1)) << dec << "\n\t"
            << options.num_churn_pages << " Churn pages from " << hex << churn_pages << " - " << (void*)((char*)churn_pages+((options.num_churn_pages*pagesize)-1)) << dec << "\n\t"
            << options.num_churn_threads << " Churn threads\n\t"
            << options.num_load_reader_threads << " Load Reader threads\n\t"
            << options.num_load_writer_threads << " Load Writer threads\n\t"
            << options.fn << " Backing file\n\t"
            << options.testduration << " seconds for test duration.\n\n";

        check();
    }

    ~pageiotest( void ) {
        utility::unmap_file(umt_options.usemmap,
            (options.num_churn_pages + num_rw_load_pages
             + num_read_load_pages) * pagesize, base_addr);
    }

    void start( void ) {
        if (options.initonly)
            return;

        for (uint64_t t = 0; t < options.num_load_reader_threads; ++t)
          load_readers.push_back(new thread{&pageiotest::load_read, this, t});

        for (uint64_t t = 0; t < options.num_load_reader_threads; ++t)
          load_rw_readers.push_back(new thread{&pageiotest::load_rw_read, this, t});

        for (uint64_t t = 0; t < options.num_load_writer_threads && t < 1; ++t)
          load_rw_writers.push_back(new thread{&pageiotest::load_rw_write, this});

        for (uint64_t t = 0; t < options.num_churn_threads; ++t)
            churn_readers.push_back(new thread{&pageiotest::churn, this, t});
    }

    void run( void ) {
        if (options.initonly)
            return;

        this_thread::sleep_for(seconds(options.testduration));
    }

    void stop( void ) {
        time_to_stop = true;
        for (auto i : load_readers)
          i->join();
        for (auto i : load_rw_readers)
          i->join();
        for (auto i : load_rw_writers)
          i->join();
        for (auto i : churn_readers)
          i->join();
    }

private:
    bool time_to_stop;
    utility::umt_optstruct_t umt_options;
    options_t options;

    long pagesize;
    void* base_addr;

    void* read_load_pages;
    void* rw_load_pages;
    void* churn_pages;

    vector<thread*> load_readers;
    vector<thread*> load_rw_readers;
    vector<thread*> load_rw_writers;
    vector<thread*> churn_readers;

    uint64_t num_churn_pages;
    uint64_t num_read_load_pages;
    uint64_t num_rw_load_pages;

    void check() {
        cout << "Checking churn load pages\n";
        {
          uint64_t* p = (uint64_t*)churn_pages;
          for (uint64_t i = 0; i < num_churn_pages * (pagesize/sizeof(*p)); i += (pagesize/sizeof(*p)))
              if (p[i] != i) {
                cerr << "check(CHURN): *(uint64_t*)" << &p[i] << "=" << p[i] << " != " << (unsigned long long)i << endl;
                exit(1);
              }
        }
        cout << "Checking read load pages\n";
        {
          uint64_t* p = (uint64_t*)(uint64_t)read_load_pages;
          for (uint64_t i = 0; i < num_read_load_pages * (pagesize/sizeof(*p)); ++i)
              if (p[i] != i) {
                cerr << "check(READER): *(uint64_t*)" << &p[i] << "=" << p[i] << " != " << (unsigned long long)i << endl;
                exit(1);
              }
        }
        cerr << "Check Complete\n";
    }

    void init() {
        cout << "Initializing churn pages\n";
        {
            uint64_t* p = (uint64_t*)churn_pages;
#pragma omp parallel for
            for (uint64_t i = 0; i < num_churn_pages * (pagesize/sizeof(*p)); ++i)
                p[i] = i;
        }

        cout << "Initializing read load pages pages\n";
        {
          uint64_t* p = (uint64_t*)(uint64_t)read_load_pages;
#pragma omp parallel for
          for (uint64_t i = 0; i < num_read_load_pages * (pagesize/sizeof(*p)); ++i)
              p[i] = i;
        }

        cout << "Initializing rw load pages pages\n";
        {
          uint64_t* p = (uint64_t*)(uint64_t)rw_load_pages;
#pragma omp parallel for
          for (uint64_t i = 0; i < num_rw_load_pages * (pagesize/sizeof(*p)); ++i)
              p[i] = i;
        }
        cerr << "Initialization Complete\n";
    }

    mutex lock;

    uint64_t churn( int tnum ) {
        uint64_t cnt = 0;
        uint64_t idx;
        uint64_t* p = (uint64_t*)churn_pages;
        mt19937 gen(tnum);
        uniform_int_distribution<uint64_t> rnd_int(0, ((num_churn_pages*(pagesize/sizeof(*p)))-1));

        while ( !time_to_stop ) {
            idx = rnd_int(gen);
            if (p[idx] != idx) {
                lock.lock();
                cerr << hex << "churn()    " << p[idx] << " != " << idx << " at address " << &p[idx] << endl;
                lock.unlock();
                break;
            }
        }
        return cnt;
    }

    void load_read(int tnum) {
        uint64_t* p = (uint64_t*)read_load_pages;
        tnum = tnum + 2048;
        mt19937 gen(tnum);
        uniform_int_distribution<uint64_t> rnd_int(0, ((num_read_load_pages*(pagesize/sizeof(*p)))-1));

        while ( !time_to_stop ) {
            uint64_t idx = rnd_int(gen);

            if (p[idx] != idx) {
                lock.lock();
                cerr << "load_read  *(uint64_t*)" << &p[idx] << "=" << p[idx] << " != " << idx << endl;
                lock.unlock();
                break;
            }
        }
    }

    // Have a reader going nuts on the write page for fun. No data validation since the writer is changing it from underneath us.
    void load_rw_read(int tnum) {
        uint64_t* p = (uint64_t*)rw_load_pages;
        tnum = tnum + tnum * 53;
        mt19937 gen(tnum);
        uniform_int_distribution<uint64_t> rnd_int(0, ((num_rw_load_pages*(pagesize/sizeof(*p)))-1));

        while ( !time_to_stop ) {
            uint64_t idx = rnd_int(gen);
            g_count += p[idx];
        }
    }

    void load_rw_write() {
        uint64_t cnt = 0;
        uint64_t* p = (uint64_t*)((uint64_t)rw_load_pages);
        const int num_entries = num_rw_load_pages * pagesize/sizeof(*p);

        omp_set_num_threads(options.num_load_writer_threads);

        while ( !time_to_stop ) {
            uint64_t cnt_base = cnt;

#pragma omp parallel for
            for (int i = 0; i < num_entries; ++i) {
                p[i] = cnt_base + i;
            }

#pragma omp parallel for
            for (int i = 0; i < num_entries; ++i) {
                if (p[i] != (cnt_base + i)) {
#pragma omp critical
                    {
                        lock.lock();
                        cerr << "load_rw_write *(uint64_t*)" << &p[i] << "=" << p[i] << " != " << (cnt_base+i) << ": (" << cnt_base << "+" << i << "=" << (cnt_base+i) << ") - " << p[i] << " = " << ((cnt_base+i)-p[i]) << endl;
                        if (i != 0)
                            cerr << "load_rw_write *(uint64_t*)" << &p[0] << "=" << p[0] << " ,  " << (cnt_base+0) << ": (" << cnt_base << "+" << 0 << "=" << (cnt_base+0) << ") - " << p[0] << " = " << ((cnt_base+0)-p[0]) << endl;
                        lock.unlock();
                    }
                    exit(1);
                }
            }

            cnt += pagesize/sizeof(*p);
        }
    }
};

int main(int argc, char **argv)
{
    pageiotest test{argc, argv};

    test.start();
    test.run();
    test.stop();

    return 0;
}
