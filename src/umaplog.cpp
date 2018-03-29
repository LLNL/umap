/* This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <mutex>
#include "umaplog.h"            // umap_log()

using namespace std;

static std::mutex mtx;
bool umap_logging = true;

void umaplog_lock(void)
{
    mtx.lock();
}

void umaplog_unlock(void)
{
    mtx.unlock();
}

void __umaplog_init(void)
{
    char *log = getenv("UMAP_LOGGING");
    if (log && atoi(log))
        umap_logging = true;
    else
        umap_logging = false;
}
