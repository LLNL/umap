#############################################################################
# Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
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
      UmapServiceCommon.hpp
      store/StoreFile.h
      store/SparseStore.h
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
    UmapServiceCommon.cpp
    store/Store.cpp
    store/StoreFile.cpp
    store/SparseStore.cpp
    util/Exception.cpp
    util/Logger.cpp
    ${umapheaders})

set(umapclientheaders
    umap-client.h
    UmapClientManager.hpp
    UmapServiceCommon.hpp
    socket.hpp)
set(umapclientsrc
    UmapClientManager.cpp
    UmapServiceCommon.cpp
    socket.cpp
    ${umapclientheaders})

find_package(Threads REQUIRED)
add_library(umap SHARED ${umapsrc} )
add_library(umapclient SHARED ${umapclientsrc} )
add_library(umap-static STATIC ${umapsrc} )
add_library(umapclient-static STATIC ${umapclientsrc} )
set_target_properties(umap-static PROPERTIES OUTPUT_NAME umap)
set_target_properties(umapclient-static PROPERTIES OUTPUT_NAME umapclient)
target_link_libraries (umap ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries (umapclient ${CMAKE_THREAD_LIBS_INIT})

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

install(TARGETS umap umap-static umapclient umapclient-static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin )

install(FILES umap.h DESTINATION include/umap )

install(FILES umap-client.h DESTINATION include/umap ) 

install(FILES store/Store.hpp DESTINATION include/umap/store )

install(FILES store/SparseStore.h DESTINATION include/umap/store )
