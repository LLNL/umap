#!/usr/bin/env zsh
#############################################################################
# Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################

setopt extended_glob

RED="\033[1;31m"
GREEN="\033[1;32m"
NOCOLOR="\033[0m"

LIC_CMD=$(which lic)
if [ ! $LIC_CMD ]; then
  echo "${RED} [!] This script requires the lic command."
  exit 255
fi

echo "Applying licenses to files"

files_no_license=$(grep -L 'This file is part of UMAP.' \
  benchmarks/**/*(^/) \
  cmake/**/*(^/) \
  docs/**/*~*rst(^/)\
  examples/**/*(^/) \
  host-configs/**/*(^/) \
  scripts/**/*(^/) \
  src/**/*~*tpl*(^/) \
  tests/**/*(^/) \
  CMakeLists.txt)

echo $files_no_license | xargs $LIC_CMD -f scripts/umap-license.txt 

echo "${GREEN} [Ok] License text applied. ${NOCOLOR}"
