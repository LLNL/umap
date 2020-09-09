#!/bin/bash

EXE=./tests/remote_stream/remote_stream

KB=1024
MB=$((1024*KB))
GB=$((1024*MB))

numPeriods=5

for g in 64  #1 8 16 
do
    regionSize=$(( GB * g ))

    # start the server on one compute node
    # start clients on a separate compute node
    for numServerNodes in 2;do
	for numServerProcs in 4;do
	    
	    rm -rf serverfile
	    export OMP_NUM_THREADS=$((24/numServerProcs))
	    srun --ntasks-per-node=$numServerProcs -N $numServerNodes ${EXE}_server $regionSize &

	    while [ ! -f serverfile ]; do
		sleep 10
	    done
	    sleep 10

	    for numClientNodes in 4;do
		for numClientProcs in 1;do
		    for bufpages in 131072 262144 524288;do
			psize=$(( 128 * KB ))	    
			numThreads=$(( 24/numClientProcs ))
			cmd="UMAP_PAGESIZE=$psize UMAP_BUFSIZE=$bufpages OMP_NUM_THREADS=$numThreads srun --ntasks-per-node=$numClientProcs -N $numClientNodes ${EXE}_client $regionSize $numPeriods"
			echo $cmd
			time eval $cmd
			rm *.core
		    done
		done
	    done
	    pkill srun
	    sleep 10
	done
    done
done
echo 'Done'
