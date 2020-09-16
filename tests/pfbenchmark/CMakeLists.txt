#############################################################################
# Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################

project(pfbenchmark)

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")

    add_executable(pfbenchmark-read pfbenchmark.cpp)
    add_executable(pfbenchmark-write pfbenchmark.cpp)
    add_executable(pfbenchmark-readmodifywrite pfbenchmark.cpp)
    add_executable(nvmebenchmark-read nvmebenchmark.cpp)
    add_executable(nvmebenchmark-write nvmebenchmark.cpp)

    if(STATIC_UMAP_LINK)
       set(umap-lib "umap-static")
    else()
       set(umap-lib "umap")
    endif()

    add_dependencies(pfbenchmark-read ${umap-lib} )
    add_dependencies(pfbenchmark-write ${umap-lib} )
    add_dependencies(pfbenchmark-readmodifywrite ${umap-lib} )
    add_dependencies(nvmebenchmark-read ${umap-lib} )
    add_dependencies(nvmebenchmark-write ${umap-lib} )

    target_link_libraries(pfbenchmark-read ${umap-lib} )
    target_link_libraries(pfbenchmark-write ${umap-lib} )
    target_link_libraries(pfbenchmark-readmodifywrite ${umap-lib} )
    target_link_libraries(nvmebenchmark-read ${umap-lib} )
    target_link_libraries(nvmebenchmark-write ${umap-lib} )

    include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS} )

    install(TARGETS pfbenchmark-read pfbenchmark-write pfbenchmark-readmodifywrite nvmebenchmark-read nvmebenchmark-write
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )
else()
  message("Skipping pfbenchmark, OpenMP required")
endif()


