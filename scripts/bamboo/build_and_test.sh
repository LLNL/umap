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

cd `dirname $0`

export UMAP_DIR=$(git rev-parse --show-toplevel)
export BUILD_DIR=build-${SYS_TYPE}

export COMPILER=${1:-gcc_4_8_5}
export BUILD_TYPE=${2:-Release}
export BUILD_OPTIONS="-DENABLE_STATS=On -DENABLE_CFITS=On -DENABLE_FITS_TESTS=On -DCFITS_LIBRARY_PATH=/g/g0/martymcf/.bin/cfitsio/lib -DCFITS_INCLUDE_PATH=/g/g0/martymcf/.bin/cfitsio/include ${BUILD_OPTIONS}"

mkdir ${BUILD_DIR} 2> /dev/null
cd ${BUILD_DIR}

echo "Configuring..."

echo "cmake -C ${UMAP_DIR}/host-configs/${SYS_TYPE}/${COMPILER}.cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ${BUILD_OPTIONS} ${UMAP_DIR}"
cmake -C ${UMAP_DIR}/host-configs/${SYS_TYPE}/${COMPILER}.cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ${BUILD_OPTIONS} ${UMAP_DIR}

echo "Building..."
make VERBOSE=1 -j

echo "Testing..."
./tests/churn/churn --directio -f /tmp/regression_test_churn.dat -b 10000 -c 20000 -l 1000 -d 10
./tests/umapsort/umapsort -p 100000 -b 95000 -f /tmp/regression_test_sort.dat --directio -t 48
/bin/rm -f /tmp/regression_test_churn.dat /tmp/regression_test_sort.dat

# if [[ $HOSTNAME == *manta* ]]; then
  # bsub -x -n 1 -G guests -Ip ctest -T Test
# else
  # srun -ppdebug -t 5 -N 1 ctest -T Test
# fi
