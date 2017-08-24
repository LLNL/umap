File=/mnt/intel/xiao/asteroid_sim_epoch
File1=/mnt/intel/xiao/real/asteroid_sim_epoch
#env LD_LIBRARY_PATH=/home/liu61/qfits/lib ~/develop/install/bin/private -f $File -p 1050000 -n 50 -b 100000 -t 8
env LD_LIBRARY_PATH=/home/liu61/qfits/lib ~/develop/install/bin/private -f $File1 -p 41000 -n 10 -b 100000 -t 8
#r -f /mnt/intel/xiao/asteroid_sim_epoch -p 1050000 -n 50 -b 100000 -t 8
