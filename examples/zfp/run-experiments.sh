#!/bin/bash

#if [ -z "$UMAP_ROOT" ];
#then
#    echo "UMAP_ROOT is not set."
#    exit
#fi

#if [ -z "$UMAP_INSTALL_PATH" ];
#then
#    echo "UMAP_INSTALL_PATH is not set. "
#    exit
#fi

#CONFIG="LD_LIBRARY_PATH=${UMAP_INSTALL_PATH}/lib:${UMAP_INSTALL_PATH}/zfp/src/ZFP-build/lib"

CONFIG=""

step=10

#for n in 56 120 248 504
for n in 248 504
do
    echo "IEEE 64"
#    for threads in 1 2 4 8 16
    for threads in 24 48  
    do
        cmd="env ${CONFIG} OMP_NUM_THREADS=${threads} ./poisson -n ${n} ${n} ${n} -t ${step} 2>& 1 | tail -n 4"
        echo $cmd
        eval $cmd
    done

    for rate in 4 8 12 16
    do
        echo "ZFP fixed rate ${rate}"
        for layers in 4 8 16 
        do
            cache=$(((${n} + 8) * (${n} + 8) * ${layers} * 8))
#            for threads in 1 2 4 8 16
            for threads in 24 48
            do
                cmd="env ${CONFIG} OMP_NUM_THREADS=${threads} ./poisson -n ${n} ${n} ${n} -t ${step} -r ${rate} -c ${cache} 2>&1 | tail -n 4"
                echo $cmd
                eval $cmd
            done
        done
    done

    for rate in 4 8 12 16
    do
        echo "ZFP fixed rate ${rate} with Umap backend"
        for layers in 4 8 16
        do
            cache=$(((${n} + 8) * (${n} + 8) * ${layers} * 8))
            blocks=$((${cache} / (8 * 64)))
            for threads in 24 48
            do
                bufsize=$(awk "BEGIN { print int(2 * ((${n} + 8)^3 * ${rate} + (${threads} + 1) * 2^int(log(${blocks}) / log(2) + (1 - 1e-9)) * (32 + 64 * 64)) / (8 * 16384)) }")
                cmd="env ${CONFIG} OMP_NUM_THREADS=${threads} UMAP_PAGE_FILLERS=8 UMAP_PAGE_EVICTORS=4 UMAP_PAGESIZE=16384 UMAP_BUFSIZE=${bufsize} ./poisson -n ${n} ${n} ${n} -t ${step} -r ${rate} -u 2>&1 | tail -n 4"
                echo $cmd
            done
        done
    done
done
