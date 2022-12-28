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


if (UMAP_ENABLE_ZFP)
   include(ExternalProject)
   # build ZFP from the official github
   ExternalProject_Add(ZFP
     PREFIX ${CMAKE_BINARY_DIR}/zfp
     GIT_REPOSITORY https://github.com/LLNL/ZFP.git
     GIT_TAG 1.0.0
#     DOWNLOAD_COMMAND      ""
     DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/zfp
     PATCH_COMMAND cp ${CMAKE_SOURCE_DIR}/examples/zfp/array3umap.hpp ${CMAKE_BINARY_DIR}/zfp/src/ZFP/include/zfp
     UPDATE_COMMAND ""
     CONFIGURE_COMMAND ""
     BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/zfp/src/ZFP && make -j
     BUILD_ALWAYS FALSE
     INSTALL_COMMAND ""
     TEST_COMMAND ""
     
   )

   set(zfpheaders
     store/StoreZFP.h
   )	 
   set(zfpsrcs
     store/StoreZFP.cpp
   )

   list(APPEND umapheaders ${zfpheaders})
   list(APPEND umapsrc     ${zfpsrcs})
   add_library(umap SHARED ${umapsrc} )
   add_library(umap-static STATIC ${umapsrc} )
   add_dependencies( umap ZFP )
   add_dependencies( umap-static ZFP )
   
   include_directories("${CMAKE_BINARY_DIR}/zfp/src/ZFP/include")

   set_target_properties(umap-static PROPERTIES OUTPUT_NAME umap)
   target_link_libraries (umap ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_BINARY_DIR}/zfp/src/ZFP/lib/libzfp.a)
else(UMAP_ENABLE_ZFP)
   add_library(umap SHARED ${umapsrc} )
   add_library(umap-static STATIC ${umapsrc} )
   set_target_properties(umap-static PROPERTIES OUTPUT_NAME umap)
   target_link_libraries (umap ${CMAKE_THREAD_LIBS_INIT})

endif(UMAP_ENABLE_ZFP)


set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

#add_definitions(-DPROF)

install(TARGETS umap umap-static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin )

install(FILES umap.h DESTINATION include/umap)

install(FILES store/Store.hpp DESTINATION include/umap/store )

install(FILES store/SparseStore.h DESTINATION include/umap/store)

install(FILES store/StoreZFP.h DESTINATION include/umap/store)
