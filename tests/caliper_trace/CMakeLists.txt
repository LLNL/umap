#############################################################################
# Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################
project(caliper_trace)

find_package(caliper REQUIRED PATHS "${caliper_DIR}")
if(caliper_FOUND)
  message(STATUS "Found caliper_INCLUDE_DIR ${caliper_INCLUDE_DIR}" )

  FIND_PACKAGE( OpenMP REQUIRED )
  if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS 
      "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    add_executable(caliper_trace caliper_trace.cpp)
    
    if(STATIC_UMAP_LINK)
      set(umap-lib "umap-static")
    else()
      set(umap-lib "umap")
    endif()
  
    add_dependencies(caliper_trace ${umap-lib})
    target_link_libraries(caliper_trace ${umap-lib} caliper) 
    include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS} ${caliper_INCLUDE_DIR})

    install(TARGETS caliper_trace
      LIBRARY DESTINATION lib
      ARCHIVE DESTINATION lib/static
      RUNTIME DESTINATION bin )
    
  else()
    message("Skipping caliper_trace, OpenMP required")
  endif()

else()
  message("Skipping caliper_trace, Caliper package not found")
endif()

