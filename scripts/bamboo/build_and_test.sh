#!/bin/bash
##############################################################################
# This file is part of UMAP.  For copyright information see the COPYRIGHT file in the top level directory, or at
# https://github.com/LLNL/umap/blob/master/COPYRIGHT This program is free software; you can redistribute it and/or 
# modify it under the terms of the GNU Lesser General Public License (as published by the Free Software Foundation) 
# version 2.1 dated February 1999.  This program is distributed in the hope that it will be useful, but WITHOUT ANY 
# WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
# and conditions of the GNU Lesser General Public License for more details.  You should have received a copy of the 
# GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 
# 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
##############################################################################

function trycmd
{
  echo $1
  $1

  if [ $? -ne 0 ]; then
    echo "Error"
    exit -1
  fi
}

cd `dirname $0`

export UMAP_DIR=$(git rev-parse --show-toplevel)
export BUILD_DIR=build-${SYS_TYPE}

export COMPILER=${1:-gcc_4_8_5}
export BUILD_TYPE=${2:-Release}
export BUILD_OPTIONS="-DENABLE_STATS=On -DENABLE_CFITS=On -DENABLE_FITS_TESTS=On -DCFITS_LIBRARY_PATH=/g/g0/martymcf/.bin/cfitsio/lib -DCFITS_INCLUDE_PATH=/g/g0/martymcf/.bin/cfitsio/include ${BUILD_OPTIONS}"

mkdir ${BUILD_DIR} 2> /dev/null
cd ${BUILD_DIR}

#cmd="cmake -C ${UMAP_DIR}/host-configs/${SYS_TYPE}/${COMPILER}.cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ${BUILD_OPTIONS} ${UMAP_DIR}"
trycmd "cmake -C ${UMAP_DIR}/host-configs/${SYS_TYPE}/${COMPILER}.cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ${BUILD_OPTIONS} ${UMAP_DIR}"

trycmd "make -j"

trycmd "./tests/churn/churn --directio -f /tmp/regression_test_churn.dat -b 10000 -c 20000 -l 1000 -d 10"
trycmd "./tests/umapsort/umapsort -p 100000 -b 95000 -f /tmp/regression_test_sort.dat --directio -t 16"
/bin/rm -f /tmp/regression_test_churn.dat /tmp/regression_test_sort.dat

# Test for median calculation using fits files
trycmd "tar -xvf $UMAP_DIR/tests/median_calculation/data/test_fits_files.tar.gz -C /tmp/"
trycmd "./tests/median_calculation/test_median_calculation -f /tmp/test_fits_files/asteroid_sim_epoch"
/bin/rm -rf /tmp/test_fits_files

trycmd "./tests/bfs/ingest_edge_list -g /tmp/test_graph $UMAP_DIR/tests/bfs/data/edge_list_rmat_s10_0_of_4 $UMAP_DIR/tests/bfs/data/edge_list_rmat_s10_1_of_4 $UMAP_DIR/tests/bfs/data/edge_list_rmat_s10_2_of_4 $UMAP_DIR/tests/bfs/data/edge_list_rmat_s10_3_of_4"
trycmd "./tests/bfs/test_bfs -n 1017 -m 32768 -g /tmp/test_graph -l $UMAP_DIR/tests/bfs/data/bfs_level_reference"

/bin/rm -f /tmp/test_graph
