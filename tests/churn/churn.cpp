/* This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */

/*
   The idea is that we have a single "Load Page" and a set
   of N "Churn Pages" as follows:
  
   +==================================================+
   |                                                  |
   |               One Read Load Page                 |
   |                                                  |
   +--------------------------------------------------+
   |                                                  |
   |            One Write{/Read} Load Page            |
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
  
   We then have a smaller page_buffer_size that these pages will be faulted into and madvised out of via umap().
  
   The LoadPage will have a set of num_load_reader and num_load_writer threads focussed exclusively on making reads and writes to locations constrained to the Load Page.

   The the Churn Pages will have num_churn_reader threads performing random byte read accesses across all of the Churn Pages effectively causing the Load Page to be paged in and out of the smaller Page_Buffer.
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

#include "umap.h"
#include "options.h"
#include "umaptest.h"

uint64_t g_count = 0;
using namespace std;
using namespace chrono;

class pageiotest {
public:
    pageiotest(int _ac, char** _av): time_to_stop{false}, pagesize{umt_getpagesize()} {
        getoptions(options, _ac, _av);

        umt_options.iodirect = options.iodirect;
        umt_options.usemmap = options.usemmap;
        umt_options.noinit = 0;
        umt_options.fn = options.fn;

        fd = umt_openandmap(&umt_options, (options.num_churn_pages+2*options.num_load_pages)*pagesize, &base_addr);

        read_load_pages = base_addr;
        write_load_pages = (void*)((uint64_t)base_addr + (options.num_load_pages*pagesize));
        churn_pages = (void*)((uint64_t)base_addr + ((options.num_load_pages*2)*pagesize));

        if ( ! options.noinit )
            init();

        cout << "Churn Test:\n\t"
            << options.page_buffer_size << " Pages in page buffer\n\t"
            << options.num_load_pages << " Read Load (focus) pages from " << hex << read_load_pages << " - " << (void*)((char*)read_load_pages+((options.num_load_pages*pagesize)-1)) << dec << "\n\t"
            << options.num_load_pages << " Write Load (focus) pages from " << hex << write_load_pages << " - " << (void*)((char*)write_load_pages+((options.num_load_pages*pagesize)-1)) << dec << "\n\t"
            << options.num_churn_pages << " Churn pages from " << hex << churn_pages << " - " << (void*)((char*)churn_pages+((options.num_churn_pages*pagesize)-1)) << dec << "\n\t"
            << options.num_churn_threads << " Churn threads\n\t"
            << options.num_load_reader_threads << " Load Reader threads\n\t"
            << options.num_load_writer_threads << " Load Writer threads\n\t"
            << options.fn << " Backing file\n\t"
            << options.testduration << " seconds for test duration.\n\n";
    }

    ~pageiotest( void ) {
        umt_closeandunmap(&umt_options, (options.num_churn_pages+2*options.num_load_pages)*pagesize, base_addr, fd);
    }

    void start( void ) {
        if (options.initonly)
            return;

        for (uint64_t p = 0; p < options.num_load_pages; ++p) {
            for (uint64_t t = 0; t < options.num_load_reader_threads; ++t)
                load_readers.push_back(new thread{&pageiotest::load_read, this, p, t});

            for (uint64_t t = 0; t < options.num_load_writer_threads && t < 1; ++t)
                load_writers.push_back(new thread{&pageiotest::load_write, this, p});
        }

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
        for (auto i : load_writers)
            i->join();
        for (auto i : churn_readers)
            i->join();
    }

private:
    bool time_to_stop;
    umt_optstruct_t umt_options;
    options_t options;

    long pagesize;
    void* base_addr;

    void* read_load_pages;
    void* write_load_pages;
    vector<thread*> load_readers;
    vector<thread*> load_writers;

    void* churn_pages;
    vector<thread*> churn_readers;

    int fd;

    void init() {
        cout << "Initializing\n";
        {
            uint64_t* p = (uint64_t*)churn_pages;
            for (uint64_t i = 0; i < options.num_churn_pages*(pagesize/sizeof(*p)); ++i)
                p[i] = i;
        }

        {
            for (uint64_t pageno = 0; pageno < options.num_load_pages; ++pageno) {
                uint64_t* p = (uint64_t*)((uint64_t)read_load_pages+(pagesize*pageno));
                for (uint64_t i = 0; i < options.num_load_pages*(pagesize/sizeof(*p)); ++i)
                    p[i] = i;

                p = (uint64_t*)((uint64_t)write_load_pages+(pagesize*pageno));
                for (uint64_t i = 0; i < options.num_load_pages*(pagesize/sizeof(*p)); ++i)
                    p[i] = i;
            }
        }
        cout << "Initialization Complete\n";
    }

    mutex lock;

    uint64_t churn( int tnum ) {
        uint64_t cnt = 0;
        uint64_t idx;
        uint64_t* p = (uint64_t*)churn_pages;
        mt19937 gen(tnum);
        uniform_int_distribution<uint64_t> rnd_int(0, ((options.num_churn_pages*(pagesize/sizeof(*p)))-1));

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

    void load_read(uint64_t pageno, int tnum) {
        uint64_t* p = (uint64_t*)((uint64_t)read_load_pages+(pagesize*pageno));
        tnum = tnum + tnum * pageno;
        mt19937 gen(tnum);
        uniform_int_distribution<uint64_t> rnd_int(0, ((pagesize/sizeof(*p))-1));

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

    void load_write(uint64_t pageno) {
        uint64_t cnt = 0;
        uint64_t* p = (uint64_t*)((uint64_t)write_load_pages+(pagesize*pageno));
        const int num_entries = pagesize/sizeof(*p);

        omp_set_num_threads(options.num_load_writer_threads);

        while ( !time_to_stop ) {
            uint64_t cnt_base = cnt;

#pragma omp parallel for
            for (int i = 0; i < num_entries; ++i) {
                g_count += p[i];         // Read first
                p[i] = cnt_base + i;
            }

#pragma omp parallel for
            for (int i = 0; i < num_entries; ++i) {
                if (p[i] != (cnt_base + i)) {
#pragma omp critical
                    {
                        lock.lock();
                        cerr << "load_write *(uint64_t*)" << &p[i] << "=" << p[i] << " != " << (cnt_base+i) << ": (" << cnt_base << "+" << i << "=" << (cnt_base+i) << ") - " << p[i] << " = " << ((cnt_base+i)-p[i]) << endl;
                        if (i != 0)
                            cerr << "load_write *(uint64_t*)" << &p[0] << "=" << p[0] << " ,  " << (cnt_base+0) << ": (" << cnt_base << "+" << 0 << "=" << (cnt_base+0) << ") - " << p[0] << " = " << ((cnt_base+0)-p[0]) << endl;
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
    cout << g_count << endl;

    return 0;
}
