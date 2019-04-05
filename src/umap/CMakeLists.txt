#############################################################################
# Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################
project(umap_libraries)

set(umapheaders
      config.h
      Buffer.hpp
      Fillers.hpp
      Flushers.hpp
      PageFiller.hpp
      PageRegion.hpp
      PageFlusher.hpp
      WorkerPool.hpp
      WorkQueue.hpp
      Uffd.hpp
      umap.h
      store/StoreFile.h
      store/Store.hpp
      util/Exception.hpp
      util/Logger.hpp
      util/Macros.hpp)

set(umapsrc
    PageRegion.cpp
    umap.cpp
    store/Store.cpp
    store/StoreFile.cpp
    util/Exception.cpp
    util/Logger.cpp
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

install(FILES umap.h store/Store.hpp DESTINATION include/umap)
