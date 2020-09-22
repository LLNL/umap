#!/usr/bin/env zsh
#############################################################################
# Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################

# This is used for the ~*tpl* line to ignore files in bundled tpls
setopt extended_glob

autoload colors

RED="\033[1;31m"
GREEN="\033[1;32m"
NOCOLOR="\033[0m"

files_no_license=$(grep -L 'Lawrence Livermore National Security' \
  cmake/**/*(^/) \
  docs/**/*~*rst(^/)\
  config/config.h.in\
  examples/*(^/) \
  host-configs/**/*(^/) \
  scripts/**/*(^/) \
  src/**/*~*tpl*(^/) \
  tests/**/*~*csv~*xlsx~*sysinfo(^/) \
  tests/CMakeLists.txt \
  CMakeLists.txt)

if [ $files_no_license ]; then
  print "${RED} [!] Some files are missing license text: ${NOCOLOR}"
  echo "${files_no_license}"
  exit 255
else
  print "${GREEN} [Ok] All files have required license info."
  exit 0
fi
