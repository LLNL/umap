#!/bin/bash

##################################################
# This test script does not require sudo privilege 
# The tests run BFS, UMapsort, and Churn tests
# with different parameters
##################################################


if [ -z "$UMAP_ROOT" ];
then
    echo "UMAP_ROOT is not set."
    exit
fi

if [ -z "$UMAP_INSTALL_PATH" ];
then
    echo "UMAP_INSTALL_PATH is not set. use $UMAP_ROOT/build. "
    UMAP_INSTALL_PATH=$UMAP_ROOT/build
fi

export LD_LIBRARY_PATH=$UMAP_INSTALL_PATH/lib:$LD_LIBRARY_PATH

echo "##############################################"
echo "# UMapSort "
echo "##############################################"
DATA_SIZE=$(( 64*1024*1024 ))
BUF_SIZE=$((  32*1024*1024 )) 
UMAP_PSIZE=16384
UMAP_BUFSIZE=$((BUF_SIZE/UMAP_PSIZE))
data_file=./sort_perf_data

cmd="env UMAP_PAGESIZE=$UMAP_PSIZE UMAP_BUFSIZE=$UMAP_BUFSIZE ./umapsort -f $data_file -p $((DATA_SIZE/UMAP_PSIZE)) -N 1 -t 24"
echo $cmd
time sh -c "$cmd"
echo ""
exit


