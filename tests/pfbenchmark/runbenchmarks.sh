#!/bin/bash
function drop_page_cache {
  echo "Dropping page cache"
  sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
}

drop_page_cache

/bin/rm -f /mnt/intel/pfbench
echo pfbenchmark -b $((((256*1024*1024*1024)/4096))) -p $(((16*1024*1024*1024)/4096)) -t 256 -f /mnt/intel/pfbench --directio --initonly
time pfbenchmark -b $((((256*1024*1024*1024)/4096))) -p $(((16*1024*1024*1024)/4096)) -t 256 -f /mnt/intel/pfbench --directio --initonly

for j in 1 80
do
  for i in 1 2 4 8 16 32 64 128 256
  do
    drop_page_cache
    echo pfbenchmark -b $((((256*1024*1024*1024)/4096))) -p $(((16*1024*1024*1024)/4096)) --noio -t $i --noinit -u $j
    time pfbenchmark -b $((((256*1024*1024*1024)/4096))) -p $(((16*1024*1024*1024)/4096)) --noio -t $i --noinit -u $j

    drop_page_cache
    echo pfbenchmark -b $((((256*1024*1024*1024)/4096))) -p $(((16*1024*1024*1024)/4096)) -t $i -f /mnt/intel/pfbench --directio --noinit -u $j
    time pfbenchmark -b $((((256*1024*1024*1024)/4096))) -p $(((16*1024*1024*1024)/4096)) -t $i -f /mnt/intel/pfbench --directio --noinit -u $j

    drop_page_cache
    echo pfbenchmark -b $((((256*1024*1024*1024)/4096))) -p $(((16*1024*1024*1024)/4096)) -t $i -f /mnt/intel/pfbench --usemmap --noinit
    time pfbenchmark -b $((((256*1024*1024*1024)/4096))) -p $(((16*1024*1024*1024)/4096)) -t $i -f /mnt/intel/pfbench --usemmap --noinit
  done
done
