#!/bin/bash
function drop_page_cache {
  # sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
  echo 3 > /proc/sys/vm/drop_caches
}

export PATH=/home/martymcf/.session.behemoth-rhel/install/linux-rhel7-x86_64/install/bin:$PATH
buffersize=$((((80*256))))
numpages=$(((16*1024*1024*1024)/4096))

file=/mnt/xfs/pfbench

drop_page_cache
nvmebenchmark-write -b $buffersize -p $numpages -t 1 -f $file --directio -u 1

for mode in " " "--shuffle"
do
  for test in "write" "read"
  do
    for i in 1 2 4 8 16 32 64 128 256
    do
      drop_page_cache
      nvmebenchmark-$test -b $buffersize -p $numpages -t $i -f $file --directio --noinit $mode
    done
  done

  for noio in " " "--noio"
  do
    for j in 1 80
    do
      for i in 1 2 4 8 16 32 64 128 256
      do
        drop_page_cache
        pfbenchmark-$test -b $buffersize -p $numpages $noio -t $i -f $file -u $j --directio --noinit $mode
      done
    done
  done
done
