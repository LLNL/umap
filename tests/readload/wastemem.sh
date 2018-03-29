#!/bin/bash

if [ ! -d /mnt/tmpfs ]; then
  sudo mkdir -p /mnt/tmpfs
  sudo chmod go+rwx /mnt/tmpfs
  sudo mount -t tmpfs -o size=$((510*1024*1024*1024)) tmpfs /mnt/tmpfs
fi

WASTE=480

echo "Flushing Memory Cache"
sudo sync
echo 3 | sudo tee /proc/sys/vm/drop_caches

echo "Disabling swap"
sudo sync
sudo swapoff -a

if [ ! -f /mnt/tmpfs/3_${WASTE}GB ]; then
    echo dd if=/dev/zero of=/mnt/tmpfs/3_${WASTE}GB bs=4096 count=$((${WASTE}*256*1024))
    dd if=/dev/zero of=/mnt/tmpfs/3_${WASTE}GB bs=4096 count=$((${WASTE}*256*1024))
fi
exit
