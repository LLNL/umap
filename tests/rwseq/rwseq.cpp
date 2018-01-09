/* This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
#include <iostream>
//#include <cassert>
//#include <cstdint>
//#include <chrono>
#include <thread>
//#include <vector>
//#include <random>
//#include <algorithm>
//#include <utmpx.h>  // sched_getcpu()
#include <omp.h>
#include <unistd.h>
#include <assert.h>

#include "umap.h"
#include "options.h"
#include "umaptest.h"

uint64_t g_count = 0;
using namespace std;

class pageiotest {
public:
    pageiotest(int _ac, char** _av): pagesize{umt_getpagesize()} {
        getoptions(options, _ac, _av);

        umt_options.iodirect = 1;
        umt_options.usemmap = 0;
        umt_options.noinit = 0;
        umt_options.filename = options.fn;

        maphandle = umt_openandmap(&umt_options, pagesize, &base_addr);
        assert(maphandle != NULL);
    }

    ~pageiotest( void ) {
        umt_closeandunmap(&umt_options, pagesize, base_addr, maphandle);
    }

    void start( void ) {
        reader = new thread{&pageiotest::read, this};
        writer = new thread{&pageiotest::write, this};
    }

    void stop( void ) {
        reader->join();
        writer->join();
    }

private:
    thread *reader;
    thread *writer;
    thread *monitor;

    umt_optstruct_t umt_options;
    options_t options;
    long pagesize;
    void* base_addr;
    void* maphandle;

    void read( void ) {
        if (options.noread) {
            cout << "Skipping read, only writes will occur\n";
            return;
        }

        uint64_t* p = (uint64_t*)base_addr;

        cout << "Reading from: " << p << endl;
        g_count = *p;      // Won't return from this until AFTER umap handler, umap hanlder will sleep for 5 seconds
        cout << "Read of " << p << " returned " << g_count << endl;
    }

    void write( void ) {
        uint64_t* p = (uint64_t*)base_addr;

        sleep(1);
        cout << "Writing 12345678 to: " << p << endl;
        *p = 12345678;
        sleep(2);
        cout << "Writing 87654321 to: " << p << endl;
        *p = 87654321;
        sleep(10);
        cout << "Writing 1010101010 to: " << p << endl;
        *p = 1010101010;
    }
};

int main(int argc, char **argv)
{
    pageiotest test{argc, argv};
    test.start();
    test.stop();

    return 0;
}
