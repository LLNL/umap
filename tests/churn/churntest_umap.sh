#!/bin/bash
#############################################################################
# Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################
function free_mem {
  m=`grep MemFree /proc/meminfo | awk -v N=2 '{print $N}'`
  fm=$(((${m}/1024)/1024))
  echo $fm GB Free
}

function drop_page_cache {
  echo "Dropping page cache"
  echo 3 > /proc/sys/vm/drop_caches
}

function disable_swap {
  echo "Disabling swap"
  swapoff -av
}

function set_readahead {
  fs=`mount | grep intel | cut -d " " -f 1`
  blockdev --setra $readahead $fs
  ra=`blockdev --getra $fs`
  echo "Read ahead set to $ra for $fs"
}

function amounttowaste {
  m=`grep MemFree /proc/meminfo | awk -v N=2 '{print $N}'`
  echo $m
  fm=$(((${m}/1024)/1024))
  waste=$((${fm}-${memtoleave}))
  echo $fm GB Available, Wasting $waste GB
}

function setuptmpfs {
  if [ ! -d /mnt/tmpfs ]; then
    mkdir -p /mnt/tmpfs
  fi

  # Unmount / Reset of already mounted
  fs=`stat -f -c '%T' /mnt/tmpfs`

  if [ "$fs" = "tmpfs" ]; then
    echo "Resetting tmpfs"
    umount /mnt/tmpfs
  fi

  fs=`stat -f -c '%T' /mnt/tmpfs`
  if [ "$fs" != "tmpfs" ]; then
    if [ ! -d /mnt/tmpfs ]; then
      mkdir -p /mnt/tmpfs
    fi
    chmod go+rwx /mnt/tmpfs
    mount -t tmpfs -o size=600g tmpfs /mnt/tmpfs
    fs=`stat -f -c '%T' /mnt/tmpfs`
    echo "/mnt/tmpfs mounted as: $fs"
  else
    echo "Unable to reset /mnt/tmpfs, exiting"
    exit 1
  fi
}

function waste_memory {
  echo "Wasting $waste GB of memory"
  echo dd if=/dev/zero of=/mnt/tmpfs/${waste}GB bs=4096 count=$((${waste}*256*1024))
  dd if=/dev/zero of=/mnt/tmpfs/${waste}GB bs=4096 count=$((${waste}*256*1024))
}

memtoleave=$((64+6))
readahead=256

disable_swap
# setuptmpfs
drop_page_cache
# amounttowaste
# waste_memory

rm -f /mnt/intel/churn.data
cmd="./churn --usemmap --initonly --directio -d 60 -f /mnt/intel/churn.data -b $(((4*1024*1024*1024)/4096)) -l $(((512*1024*1024)/4096)) -c $(((1*1024*1024*1024)/4096)) -t 40 -r 40 -w 40"
echo $cmd
time sh -c "$cmd"

set_readahead

for t in 1 2 3 4 5 6
do
  drop_page_cache
  free_mem
  cmd="./churn --usemmap --noinit --directio -d 60 -f /mnt/intel/churn.data -b $(((1*1024*1024*1024)/4096)) -l $(((512*1024*1024)/4096)) -c $(((1*1024*1024*1024)/4096)) -t 40 -r 40 -w 40"
  date
  echo $cmd
  time sh -c "$cmd"
done

readahead=0
set_readahead

rm -f /mnt/intel/churn.data
cmd="./churn --initonly --directio -d 60 -f /mnt/intel/churn.data -b $(((4*1024*1024*1024)/4096)) -l $(((512*1024*1024)/4096)) -c $(((1*1024*1024*1024)/4096)) -t 40 -r 40 -w 40"
echo $cmd
time sh -c "$cmd"


for t in 1 2 3 4 5 6
do
  drop_page_cache
  free_mem
  cmd="./churn --noinit --directio -d 60 -f /mnt/intel/churn.data -b $(((1*1024*1024*1024)/4096)) -l $(((512*1024*1024)/4096)) -c $(((1*1024*1024*1024)/4096)) -t 40 -r 40 -w 40"
  date
  echo $cmd
  time sh -c "$cmd"
done
