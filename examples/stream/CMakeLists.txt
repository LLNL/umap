#############################################################################
# Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################
project(stream)

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS} -D_GNU_SOURCE") #-DUSE_UMAP
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS} -D_GNU_SOURCE")
  set(CMAKE_EXE_LINKER_FLAGS 
    "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/run_stream.sh"
    "${CMAKE_CURRENT_BINARY_DIR}/run_stream.sh"
    COPYONLY
  )
  add_executable(stream stream.c)

  set(CMAKE_SKIP_RPATH TRUE)
  
  if(STATIC_UMAP_LINK)
    set(umap-lib "umap-static")
  else()
    set(umap-lib "umap")
  endif()

  add_dependencies(stream ${umap-lib})
  target_link_libraries(stream ${umap-lib})

  include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS} )

  install(TARGETS stream
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
    RUNTIME DESTINATION bin )
else()
  message("Skipping stream, OpenMP required")
endif()

