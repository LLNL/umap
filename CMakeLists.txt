#############################################################################
# Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################
cmake_minimum_required (VERSION 3.5.1)
project(umap
  VERSION 2.0.0
  LANGUAGES CXX C
)

OPTION (ENABLE_TESTS "Selects whether tests are built." On)
OPTION (ENABLE_LOGGING "Build umap with Logging enabled" On)

include(cmake/BuildEnv.cmake)
include(cmake/BuildType.cmake)
include(cmake/SetupUmapThirdParty.cmake)

set(UMAP_DEBUG_LOGGING ${ENABLE_LOGGING})
configure_file(
  ${PROJECT_SOURCE_DIR}/config/config.h.in
  ${PROJECT_BINARY_DIR}/src/umap/config.h)

include_directories ( "${CMAKE_SOURCE_DIR}/src" "${CMAKE_BINARY_DIR}/src")

add_subdirectory(src)

if (ENABLE_TESTS)
  add_subdirectory(examples)
  add_subdirectory(tests)
endif()

