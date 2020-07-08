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
      EvictManager.hpp
      EvictWorkers.hpp
      FillWorkers.hpp
      PageDescriptor.hpp
      RegionManager.hpp
      RegionDescriptor.hpp
      Uffd.hpp
      umap.h
      socket.hpp
      WorkQueue.hpp
      WorkerPool.hpp
      UmapServiceManager.hpp
      store/StoreFile.h
      store/Store.hpp
      util/Exception.hpp
      util/Logger.hpp
      util/Macros.hpp)

set(umapsrc
    Buffer.cpp
    EvictManager.cpp
    EvictWorkers.cpp
    FillWorkers.cpp
    PageDescriptor.cpp
    RegionManager.cpp
    Uffd.cpp
    umap.cpp
    socket.cpp
    UmapServiceManager.cpp
    store/Store.cpp
    store/StoreFile.cpp
    util/Exception.cpp
    util/Logger.cpp
    ${umapheaders})

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

install(FILES umap.h DESTINATION include/umap)

install(FILES store/Store.hpp DESTINATION include/umap/store )
