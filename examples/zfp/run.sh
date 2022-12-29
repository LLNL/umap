#!/bin/bash

if [ -z "$UMAP_ROOT" ];
then
    echo "UMAP_ROOT is not set."
    exit
fi

CONFIG="LD_LIBRARY_PATH=${UMAP_ROOT}/lib:${UMAP_ROOT}/zfp/src/ZFP-build/lib OMP_NUM_THREADS=48 UMAP_PAGE_FILLERS=8 UMAP_PAGE_EVICTORS=4 "

psize=$(( 4096 * 4 ))
step=10

for n in 128 256
do
    cmd="env ${CONFIG} ./poisson -n ${n} ${n} ${n} -t ${step}"
    echo $cmd
    eval $cmd

    cmd="env ${CONFIG} ./poisson -n ${n} ${n} ${n} -t ${step} -r 16"
    echo $cmd
    eval $cmd

    cmd="env ${CONFIG} ./poisson -n ${n} ${n} ${n} -t ${step} -r 16 -u"
    echo $cmd
    eval $cmd

done
