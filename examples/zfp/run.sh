#!/bin/bash

if [ -z "$UMAP_ROOT" ];
then
    echo "UMAP_ROOT is not set."
    exit
fi

CONFIG="LD_LIBRARY_PATH=${UMAP_ROOT}/lib:${UMAP_ROOT}/zfp/src/ZFP-build/lib OMP_NUM_THREADS=48 UMAP_PAGE_FILLERS=8 UMAP_PAGE_EVICTORS=4 "

psize=$(( 4096 * 4 ))
step=5

for n in 256
do
    bufsize=$(( n * n * 8 * 8 / psize ))
    for exe in zfp16-umap  zfp16-original #double float
    do
	cmd="env ${CONFIG} UMAP_PAGESIZE=${psize} UMAP_BUFSIZE=${bufsize} ./poisson-${exe} ${step} $n "
	echo $cmd
	eval $cmd
    done
done
