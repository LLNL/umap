#!/bin/bash
#############################################################################
# Copyright (c) 2018, Lawrence Livermore National Security, LLC.
# Produced at the Lawrence Livermore National Laboratory
#
# Created by Marty McFadden, 'mcfadden8 at llnl dot gov'
# LLNL-CODE-733797
#
# All rights reserved.
#
# This file is part of UMAP.
#
# For details, see https://github.com/LLNL/umap
# Please also see the COPYRIGHT and LICENSE files for LGPL license.
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
