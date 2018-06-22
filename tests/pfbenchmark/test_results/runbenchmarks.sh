#!/bin/bash
function drop_page_cache {
  # sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
  echo 3 > /proc/sys/vm/drop_caches
}

/bin/rm -f /mnt/xfs/pfbench

export PATH=/home/martymcf/.session.behemoth-rhel/install/linux-rhel7-x86_64/install/bin:$PATH
buffersize=$((((80*256))))
# numpages=$(((1*1024*1024*1024)/4096))
numpages=$(((16*1024*1024*1024)/4096))

drop_page_cache
nvmebenchmark-write -b $buffersize -p $numpages -t 1 -f /mnt/xfs/pfbench --directio -u 1

for j in 1
do
  for i in 1 2 4 8 16 32 64 128 256
  do
    drop_page_cache
    nvmebenchmark-write -b $buffersize -p $numpages -t $i -f /mnt/xfs/pfbench --directio -u $j --noinit
  done
done

for j in 1
do
  for i in 1 2 4 8 16 32 64 128 256
  do
    drop_page_cache
    nvmebenchmark-read -b $buffersize -p $numpages -t $i -f /mnt/xfs/pfbench --directio -u $j --noinit
  done
done

for j in 1 80
do
  for i in 1 2 4 8 16 32 64 128 256
  do
    drop_page_cache
    pfbenchmark-write -b $buffersize -p $numpages --noio -t $i -u $j --noinit
  done
done

for j in 1 80
do
  for i in 1 2 4 8 16 32 64 128 256
  do
    drop_page_cache
    pfbenchmark-read -b $buffersize -p $numpages --noio -t $i -u $j --noinit
  done
done

for j in 1 80
do
  for i in 1 2 4 8 16 32 64 128 256
  do
    drop_page_cache
    pfbenchmark-write -b $buffersize -p $numpages -t $i -f /mnt/xfs/pfbench --directio -u $j --noinit
  done
done

for j in 1 80
do
  for i in 1 2 4 8 16 32 64 128 256
  do
    drop_page_cache
    pfbenchmark-read -b $buffersize -p $numpages -t $i -f /mnt/xfs/pfbench --directio -u $j --noinit

    # echo pfbenchmark-write -b $buffersize -p $numpages -t $i -f /mnt/xfs/pfbench --usemmap -u $j
    # pfbenchmark-write -b $buffersize -p $numpages -t $i -f /mnt/xfs/pfbench --usemmap -u $j
    # echo pfbenchmark-readmodifywrite -b $buffersize -p $numpages -t $i -f /mnt/xfs/pfbench --usemmap -u $j --noinit
    # pfbenchmark-readmodifywrite -b $buffersize -p $numpages -t $i -f /mnt/xfs/pfbench --usemmap -u $j --noinit
    # echo pfbenchmark-read -b $buffersize -p $numpages -t $i -f /mnt/xfs/pfbench --usemmap -u $j --noinit
    # pfbenchmark-read -b $buffersize -p $numpages -t $i -f /mnt/xfs/pfbench --usemmap -u $j --noinit
  done
done
