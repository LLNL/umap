# Build
Under construction...

# Run
```sh
$ ls /mnt/ssd/
asteroid_sim_epoch10.fits  asteroid_sim_epoch2.fits  asteroid_sim_epoch4.fits  asteroid_sim_epoch6.fits  asteroid_sim_epoch8.fits
asteroid_sim_epoch1.fits   asteroid_sim_epoch3.fits  asteroid_sim_epoch5.fits  asteroid_sim_epoch7.fits  asteroid_sim_epoch9.fits

$ cd /path/to/umap/build/directory/
$ env NUM_VECTORS=10000 ./tests/median_calculation/run_random_vector -f /mnt/ssd/asteroid_sim_epoch
```
