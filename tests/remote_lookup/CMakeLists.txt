#############################################################################
# Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################
project(remote_lookup)

FIND_PACKAGE( OpenMP REQUIRED )
FIND_PACKAGE( MPI REQUIRED)

if(OPENMP_FOUND)
  if(MPI_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")

    add_executable(remote_lookup_server remote_lookup_server.cpp)
    add_executable(remote_lookup_client remote_lookup_client.cpp)
    add_dependencies(remote_lookup_server umap)
    add_dependencies(remote_lookup_client umap)
    target_link_libraries(remote_lookup_server umap ${MPI_LIBRARIES})
    target_link_libraries(remote_lookup_client umap ${MPI_LIBRARIES})

    include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS} ${MPI_INCLUDE_PATH} )

    install(TARGETS remote_lookup_server remote_lookup_client 
    		    LIBRARY DESTINATION lib
    		    ARCHIVE DESTINATION lib/static
    		    RUNTIME DESTINATION bin )
  else()
    message("Skipping remote_lookup, MPI required")
  endif()
else()
  message("Skipping remote_lookup, OpenMP required")
endif()

