/* This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */

#include <stdio.h>
#include <omp.h>
#include "umap.h"
#include "umaptest.h"

uint64_t r_pages(uint64_t *pagebase, uint64_t pages, uint64_t pagesize)
{
    uint64_t skip = pagesize/sizeof(*pagebase);
    uint64_t sum = 0;

#pragma omp parallel for reduction(+:sum)
    for(uint64_t i=0; i < (pages * skip); i += skip)
        sum += pagebase[i];

    return sum; // false sharing....
}

void w_pages(uint64_t *pagebase, uint64_t pages, uint64_t pagesize)
{
    uint64_t skip = pagesize/sizeof(*pagebase);
    uint64_t sum = 0;

#pragma omp parallel for
    for(uint64_t i=0; i < (pages * skip); i += skip) {
        sum += skip;
        pagebase[i] = sum;
    }
}

void rw_pages(uint64_t *pagebase, uint64_t pages, uint64_t pagesize)
{
    uint64_t skip = pagesize/sizeof(*pagebase);

#pragma omp parallel for
    for(uint64_t i=0; i < (pages * skip); i += skip)
        pagebase[i+1] = pagebase[i];
}

int main(int argc, char **argv)
{
    umt_optstruct_t options;
    umt_getoptions(&options, argc, argv);

    long pagesize = umt_getpagesize();
    int64_t totalbytes = options.numpages*pagesize;
    void* base_addr;
    int fd = umt_openandmap(&options, totalbytes, &base_addr);

    fprintf(stderr, "%s: %lu pages, %lu threads\n", argv[0], options.numpages, options.numthreads);

    omp_set_num_threads(options.numthreads);

    fprintf(stderr, "\n\nread %lu pages:\n", options.numpages);
    (void)r_pages((uint64_t*)base_addr, options.numpages, pagesize);
    fprintf(stderr, "\n\nwrite %lu pages:\n", options.numpages);
    w_pages((uint64_t*)base_addr, options.numpages, pagesize);
    fprintf(stderr, "\n\nread+write %lu pages:\n", options.numpages);
    rw_pages((uint64_t*)base_addr, options.numpages, pagesize);

    umt_closeandunmap(&options, totalbytes, base_addr, fd);

    return 0;
}
