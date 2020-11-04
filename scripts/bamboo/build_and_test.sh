#!/bin/bash
#############################################################################
# Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################

#
# This script is intended to be run by the bamboo continuous integration
# project definition for UMAP.  It is invoked with the following command
# line arguments:
# $1 - Optionally set to compiler configuration to run
# $2 - Optionally set to -Release or -Debug build ($1 must be set)
#
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
export BUILD_OPTIONS="${BUILD_OPTIONS}"
mkdir ${BUILD_DIR} 2> /dev/null
cd ${BUILD_DIR}

trycmd "cmake -C ${UMAP_DIR}/host-configs/${SYS_TYPE}/${COMPILER}.cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ${BUILD_OPTIONS} ${UMAP_DIR}"

trycmd "make -j"

trycmd "./tests/churn/churn -f /tmp/regression_test_churn.dat -b 10000 -c 20000 -l 1000 -d 10"
trycmd "./examples/psort /tmp/regression_test_sort.dat"
/bin/rm -rf /tmp/regression_test_churn.dat /tmp/regression_test_sort.dat

