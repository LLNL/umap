#############################################################################
# Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################
project(multi_thread)

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS 
    "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
  add_executable(multi_thread multi_thread.cpp)

  
  if(STATIC_UMAP_LINK)
    set(umap-lib "umap-static")
  else()
    set(umap-lib "umap")
  endif()

  add_dependencies(multi_thread ${umap-lib})
  target_link_libraries(multi_thread ${umap-lib})

  include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS} )

  install(TARGETS multi_thread
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
    RUNTIME DESTINATION bin )
else()
  message("Skipping multi_thread, OpenMP required")
endif()

