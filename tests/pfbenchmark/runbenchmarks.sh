#!/bin/bash
function drop_page_cache {
  sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
}

drop_page_cache

/bin/rm -f /mnt/intel/pfbench

buffersize=$((((80*4))))
# numpages=$(((1*1024*1024*1024)/4096))
numpages=$(((1*1024*1024)/4096))

for j in 1 80
do
  for i in 1 2 4 8 16 32 64 128 256
  do
    echo pfbenchmark-write -b $buffersize -p $numpages --noio -t $i -u $j
    time pfbenchmark-write -b $buffersize -p $numpages --noio -t $i -u $j
    echo pfbenchmark-readmodifywrite -b $buffersize -p $numpages --noio -t $i -u $j
    time pfbenchmark-readmodifywrite -b $buffersize -p $numpages --noio -t $i -u $j
    echo pfbenchmark-read -b $buffersize -p $numpages --noio -t $i -u $j
    time pfbenchmark-read -b $buffersize -p $numpages --noio -t $i -u $j

    echo pfbenchmark-write -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --directio -u $j
    time pfbenchmark-write -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --directio -u $j
    echo pfbenchmark-readmodifywrite -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --directio -u $j
    time pfbenchmark-readmodifywrite -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --directio -u $j
    echo pfbenchmark-read -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --directio -u $j
    time pfbenchmark-read -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --directio -u $j

    # echo pfbenchmark-write -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --usemmap -u $j
    # time pfbenchmark-write -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --usemmap -u $j
    # echo pfbenchmark-readmodifywrite -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --usemmap -u $j
    # time pfbenchmark-readmodifywrite -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --usemmap -u $j
    # echo pfbenchmark-read -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --usemmap -u $j
    # time pfbenchmark-read -b $buffersize -p $numpages -t $i -f /mnt/intel/pfbench --usemmap -u $j
  done
done
