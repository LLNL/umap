#!/bin/bash
function drop_page_cache {
  sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
}

drop_page_cache

/bin/rm -f /mnt/intel/pfbench

buffersize=$((((80*256))))
# numpages=$(((1*1024*1024*1024)/4096))
numpages=$(((1*1024*1024*1024)/4096))

for j in 1 80
do
  for i in 1 2 4 8 16 32 64 128 256
  do
    drop_page_cache
    nvmebenchmark-write -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --directio -u $j
    drop_page_cache
    nvmebenchmark-read -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --directio -u $j --noinit

    drop_page_cache
    pfbenchmark-write -b $buffersize -p $numpages --noio -t $i -u $j
    drop_page_cache
    pfbenchmark-readmodifywrite -b $buffersize -p $numpages --noio -t $i -u $j
    drop_page_cache
    pfbenchmark-read -b $buffersize -p $numpages --noio -t $i -u $j

    drop_page_cache
    pfbenchmark-write -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --directio -u $j
    drop_page_cache
    pfbenchmark-readmodifywrite -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --directio -u $j --noinit
    drop_page_cache
    pfbenchmark-read -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --directio -u $j --noinit

    # echo pfbenchmark-write -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --usemmap -u $j
    # pfbenchmark-write -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --usemmap -u $j
    # echo pfbenchmark-readmodifywrite -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --usemmap -u $j --noinit
    # pfbenchmark-readmodifywrite -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --usemmap -u $j --noinit
    # echo pfbenchmark-read -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --usemmap -u $j --noinit
    # pfbenchmark-read -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --usemmap -u $j --noinit
  done
done
