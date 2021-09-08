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
add_library(mpumapd SHARED ${umapsrc} )
add_library(mpumapclient SHARED ${umapclientsrc} )
add_library(mpumapd-static STATIC ${umapsrc} )
add_library(mpumapclient-static STATIC ${umapclientsrc} )
set_target_properties(mpumapd-static PROPERTIES OUTPUT_NAME mpumapd)
set_target_properties(mpumapclient-static PROPERTIES OUTPUT_NAME mpumapclient)
target_link_libraries (mpumapd ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries (mpumapclient ${CMAKE_THREAD_LIBS_INIT})

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

install(TARGETS mpumapd mpumapd-static mpumapclient mpumapclient-static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin )

install(FILES umap.h RENAME mpumapd.h DESTINATION include/umap )

install(FILES umap-client.h RENAME mpumap-client.h DESTINATION include/umap ) 

install(FILES store/Store.hpp DESTINATION include/umap/store )

install(FILES store/SparseStore.h DESTINATION include/umap/store )
