#File=./testfiles/asteroid_sim_epoch
#env LD_LIBRARY_PATH=/home/liu61/qfits/lib ~/merge/install/bin/multiple -f $File -p 40990 -n 10
File=./testfiles/asteroid_sim_epoch1.fits
env LD_LIBRARY_PATH=/home/liu61/qfits/lib ~/merge/install/bin/FITS -f $File -p 4097
#export LD_LIBRARY_PATH
#File=./testfiles/asteroid_sim_epoch10.fits
#env LD_LIBRARY_PATH=/home/liu61/qfits/lib ~/merge/install/bin/simple -f $File -p 4097
#r -f ./testfiles/asteroid_sim_epoch -p 40990
#r -f ./testfiles/asteroid_sim_epoch1.fits -p 4097
