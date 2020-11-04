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
      WorkQueue.hpp
      WorkerPool.hpp
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
    store/Store.cpp
    store/StoreFile.cpp
    store/SparseStore.cpp
    util/Exception.cpp
    util/Logger.cpp
    ${umapheaders})

find_package(Threads REQUIRED)

add_library(umap SHARED ${umapsrc} )
add_library(umap-static STATIC ${umapsrc} )
set_target_properties(umap-static PROPERTIES OUTPUT_NAME umap)
target_link_libraries (umap ${CMAKE_THREAD_LIBS_INIT})

if (caliper_DIR)
   find_package(caliper REQUIRED)
   message(STATUS "Found caliper_INCLUDE_DIR ${caliper_INCLUDE_DIR}" )
   target_include_directories(umap PUBLIC ${caliper_INCLUDE_DIR})
   target_include_directories(umap-static PUBLIC ${caliper_INCLUDE_DIR})
   target_link_libraries(umap caliper)
   target_link_libraries(umap-static caliper)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCALIPER")
endif()

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

install(TARGETS umap umap-static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin )

install(FILES umap.h DESTINATION include/umap)

install(FILES store/Store.hpp DESTINATION include/umap/store )

install(FILES store/SparseStore.h DESTINATION include/umap/store)
