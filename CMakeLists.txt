#############################################################################
# Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################
cmake_minimum_required (VERSION 3.5.1)
project(umap
  VERSION 2.1.0
  LANGUAGES CXX C
)

OPTION (ENABLE_TESTS "Selects whether tests are built." On)
OPTION (ENABLE_LOGGING "Build umap with Logging enabled" On)
OPTION (ENABLE_DISPLAY_STATS "Display umap statistics when closing" Off)
OPTION (ENABLE_TESTS_LINK_STATIC_UMAP "Build tests statically linked to umap" Off)

include(cmake/BuildEnv.cmake)
include(cmake/BuildType.cmake)
include(cmake/SetupUmapThirdParty.cmake)

set(UMAP_DEBUG_LOGGING ${ENABLE_LOGGING})
set(UMAP_DISPLAY_STATS ${ENABLE_DISPLAY_STATS})
configure_file(
  ${PROJECT_SOURCE_DIR}/config/config.h.in
  ${PROJECT_BINARY_DIR}/src/umap/config.h)

include_directories ( "${CMAKE_SOURCE_DIR}/src" "${CMAKE_BINARY_DIR}/src")

add_subdirectory(src)

if (ENABLE_TESTS)
  #set(STATIC_UMAP_LINK ${ENABLE_TESTS_LINK_STATIC_UMAP} CACHE BOOL "Linking option for Umap binaries" FORCE)
  add_subdirectory(examples)
  add_subdirectory(tests)
endif()
