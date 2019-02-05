#############################################################################
# Copyright (c) 2019, Lawrence Livermore National Security, LLC.
# Produced at the Lawrence Livermore National Laboratory
#
# Created by Marty McFadden, 'mcfadden8 at llnl dot gov'
# LLNL-CODE-733797
#
# All rights reserved.
#
# This file is part of UMAP.
#
# For details, see https://github.com/LLNL/umap
# Please also see the COPYRIGHT and LICENSE files for LGPL license.
#############################################################################
project(psort)

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS 
    "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
  add_executable(psort psort.cpp)

  add_dependencies(psort umap)
  target_link_libraries(psort umap)

  include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS} )

  install(TARGETS psort
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
    RUNTIME DESTINATION bin )
else()
  message("Skipping psort, OpenMP required")
endif()

