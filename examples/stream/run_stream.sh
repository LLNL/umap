#!/bin/bash

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

umap_psize=8192
array_length=8192
umap_bufsize=$(( array_length * 8 * 3 / umap_psize / 2 ))

# init data array
cmd="env LD_LIBRARY_PATH=${UMAP_INSTALL_PATH}/lib OMP_NUM_THREADS=2 UMAP_PAGESIZE=$umap_psize UMAP_BUFSIZE=$umap_bufsize ./stream $array_length 1"
echo $cmd
date
eval $cmd

# perform streaming tests
cmd="env LD_LIBRARY_PATH=${UMAP_INSTALL_PATH}/lib OMP_NUM_THREADS=2 UMAP_PAGESIZE=$umap_psize UMAP_BUFSIZE=$umap_bufsize ./stream $array_length 0"
echo $cmd
date
eval $cmd

echo "Done"
exit