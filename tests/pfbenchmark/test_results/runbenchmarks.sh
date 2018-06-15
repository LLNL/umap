#!/bin/bash
function drop_page_cache {
  sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
}

drop_page_cache

/bin/rm -f /mnt/intel/pfbench

buffersize=$((((256*1024*1024*1024)/4096)))
numpages=$(((16*1024*1024*1024)/4096))

for j in 1 80
do
  for i in 1 2 4 8 16 32 64 128 256
  do
    drop_page_cache
    echo pfbenchmark -b $buffersize -p $numpages --noio -t $i -u $j
    time pfbenchmark -b $buffersize -p $numpages --noio -t $i -u $j

    drop_page_cache
    echo pfbenchmark -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --directio -u $j
    time pfbenchmark -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --directio -u $j

    drop_page_cache
    echo pfbenchmark -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --usemmap
    time pfbenchmark -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --usemmap
  done
done
