#############################################################################
# Copyright (c) 2018, Lawrence Livermore National Security, LLC.
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
project(churn)

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/churntest_umap.sh"
      "${CMAKE_CURRENT_BINARY_DIR}/churntest_umap.sh"
      COPYONLY
      )
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    add_executable(churn churn.cpp options.cpp)

    add_dependencies(churn umap)
    target_link_libraries(churn umap)

    include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS} )

    install(TARGETS churn
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )
else()
  message("Skipping churn, OpenMP required")
endif()

