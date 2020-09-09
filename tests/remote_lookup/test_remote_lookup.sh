#!/bin/bash

EXE=./tests/remote_lookup/remote_lookup

KB=1024
MB=$((1024*KB))
GB=$((1024*MB))

numUpdates=10000
numPeriods=100

for g in 128
do
    regionSize=$(( GB * g ))

    # start the server on one compute node
    # start clients on a separate compute node
    for numServerNodes in 2;do
	for numServerProcPerNode in 4 8;do
	    rm -rf serverfile
	    export OMP_NUM_THREADS=$(( 24/numServerProcPerNode ))
	    cmd="UMAP_PAGESIZE=1048576 srun --ntasks-per-node=$numServerProcPerNode -N $numServerNodes ${EXE}_server $regionSize & "
	    echo $cmd
	    eval $cmd

	    # start clients after the server has published their ports
	    while [ ! -f serverfile ]; do
		sleep 10
	    done
	    sleep 10
	    
	    for numClientNodes in 4;do
		for numClientProcPerNode in 1;do
		    for bufpages in 1048576 2097152 4194304;do
			psize=$(( 16 * KB ))
			numClientThreads=$(( 24/numClientProcPerNode ))
			cmd="UMAP_PAGESIZE=$psize UMAP_BUFSIZE=$bufpages OMP_NUM_THREADS=$numClientThreads srun --ntasks-per-node=$numClientProcPerNode -N $numClientNodes ${EXE}_client $regionSize $numUpdates $numPeriods"
			echo ""
			echo $cmd
			time eval $cmd
		    done
		done
	    done
	    pkill srun
	    sleep 3
	done
    done    
done
echo 'Done'
