#############################################################################
# Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################
# Set a default build type if none was specified
set( default_build_type "Release" )
if ( EXISTS "${CMAKE_SOURCE_DIR}/.git" )
  set( default_build_type "Debug" )
endif()

if ( NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES )
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
      "MinSizeRel" "RelWithDebInfo")
endif()
