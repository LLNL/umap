#!/bin/bash

if [ -z "$UMAP_ROOT" ];
then
    echo "UMAP_ROOT is not set."
    exit
fi

if [ -z "$UMAP_INSTALL_PATH" ];
then
    echo "UMAP_INSTALL_PATH is not set. "
    exit
fi

CONFIG="LD_LIBRARY_PATH=${UMAP_INSTALL_PATH}/lib:${UMAP_INSTALL_PATH}/zfp/src/ZFP-build/lib OMP_NUM_THREADS=48 "

step=3

for n in 512
do
    echo "IEEE 64"
    cmd="env ${CONFIG} ./poisson -n ${n} ${n} ${n} -t ${step}"
    echo $cmd
    eval $cmd

    echo "ZFP fixed rate 16"
    cmd="env ${CONFIG} ./poisson -n ${n} ${n} ${n} -t ${step} -r 16"
    echo $cmd
    eval $cmd

    echo "ZFP fixed rate 16 with Umap backend"
    cmd="env ${CONFIG} UMAP_PAGE_FILLERS=8 UMAP_PAGE_EVICTORS=4 UMAP_PAGESIZE=16384 ./poisson -n ${n} ${n} ${n} -t ${step} -r 16 -u"
    echo $cmd
    eval $cmd

done
