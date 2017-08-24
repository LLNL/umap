File=/mnt/intel/xiao/asteroid_sim_epoch
#env LD_LIBRARY_PATH=/home/liu61/qfits/lib ~/develop/install/bin/multiple -f $File -p 52429000 -n 50 -b 10000000 -t 8
File1=/mnt/intel/xiao/real/asteroid_sim_epoch
env LD_LIBRARY_PATH=/home/liu61/qfits/lib ~/develop/install/bin/multiple -f $File1 -p 41000 -n 10 -b 100000 -t 8
#r -f /mnt/intel/xiao/asteroid_sim_epoch -p 52429000 -n 50 -b 10000000 -t 16
#env LD_LIBRARY_PATH=/home/liu61/qfits/lib ~/develop/install/bin/multiple -f $File -p 52429000 -n 50 -b 10 -t 1
#r -f /mnt/intel/xiao/real/asteroid_sim_epoch -p 41000 -n 10 -b 100000 -t 8
