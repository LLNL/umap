#############################################################################
# Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################
project(umap_libraries)

set(umapheaders
      config.h
      Exception.hpp
      Logger.hpp
      Macros.hpp
      StoreFile.h
      Store.h
      umap.h)

set(umapsrc
    Exception.cpp
    Logger.cpp
    Store.cpp
    StoreFile.cpp
    umap.cpp
    ${umapheaders})

if ( ENABLE_LOGGING )
endif()

find_package(Threads REQUIRED)
add_library(umap SHARED ${umapsrc} )
add_library(umap-static STATIC ${umapsrc} )
set_target_properties(umap-static PROPERTIES OUTPUT_NAME umap)
target_link_libraries (umap ${CMAKE_THREAD_LIBS_INIT})

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

install(TARGETS umap umap-static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin )

install(FILES umap.h Store.h DESTINATION include/umap)
