#!/bin/bash
#############################################################################
# Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################

function drop_page_cache {
  # sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
  echo 3 > /proc/sys/vm/drop_caches
}

export PATH=/home/martymcf/.sessions/dst-intel/install/linux-rhel7-x86_64/install/bin:$PATH
buffersize=$((((128*256))))
# numpages=$(((16*1024*1024*1024)/4096))
numpages=$(((1*1024*1024*1024*1024)/4096))

# numaccesspages=0
numaccesspages=$(((32*1024*1024*1024)/4096))

file=/mnt/xfs/pfbench

# drop_page_cache
# nvmebenchmark-write -b $buffersize -p $numpages -t 1 -f $file --directio -u 1

for test in "write" "read"
do
  for i in 16 32 64 128 256
  do
    drop_page_cache
    nvmebenchmark-$test -b $buffersize -p $numpages -t $i -f $file --directio --noinit --shuffle -a $numaccesspages
  done

  for noio in " " "--noio"
  do
    for j in 32 64 128
    do
      for i in 16 32 64 128 256
      do
        drop_page_cache
        pfbenchmark-$test -b $buffersize -p $numpages $noio -t $i -f $file -u $j --directio --noinit --shuffle -a $numaccesspages
      done
    done
  done
done
