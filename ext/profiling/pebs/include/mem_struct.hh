#ifndef PROF_MEM_STRUCT_H
#define PROF_MEM_STRUCT_H

#include <iostream>
#include <boost/lockfree/queue.hpp>
#include "conf.hh"

using namespace std;
using namespace boost::lockfree;

struct memory_address
{
    void *start_address;
    size_t offset;
};

struct metrics
{
    memory_address ma;
    size_t bytes;
    atomic<long>* samples;
};

#endif
