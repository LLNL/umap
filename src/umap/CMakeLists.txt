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


include(ExternalProject)
if (USE_ZFP)

   # build ZFP from the official github
   
   ExternalProject_Add(ZFP
	PREFIX ${CMAKE_BINARY_DIR}/zfp
      	#GIT_REPOSITORY https://github.com/LLNL/ZFP.git
	#GIT_TAG 0.5.3
	#GIT_SHALLOW 1
	URL https://github.com/LLNL/zfp/archive/refs/tags/0.5.1.zip
      	BUILD_COMMAND make -j
      	BUILD_IN_SOURCE 1
	BUILD_ALWAYS 1
      	LOG_BUILD 0
      	INSTALL_COMMAND "make install"
      	LOG_INSTALL 0
   )

   set(zfpheaders
	store/StoreZFP.h
   )	 
   set(zfpsrcs
    store/StoreZFP.cpp
   )

   list(APPEND umapheaders ${zfpheaders})
   list(APPEND umapsrc ${zfpheaders} ${zfpsrcs})
   add_library(umap SHARED ${umapsrc} )
   add_library(umap-static STATIC ${umapsrc} )
   add_dependencies( umap ZFP )
   
   ExternalProject_Get_Property(ZFP source_dir)
   message(${source_dir})
   include_directories(${source_dir}/include)
   include_directories(${source_dir}/array)

   set_target_properties(umap-static PROPERTIES OUTPUT_NAME umap)
   target_link_libraries (umap ${CMAKE_THREAD_LIBS_INIT} ${install_dir}/lib/libzfp.so)

else(USE_ZFP)

   add_library(umap SHARED ${umapsrc} )
   add_library(umap-static STATIC ${umapsrc} )
   set_target_properties(umap-static PROPERTIES OUTPUT_NAME umap)
   target_link_libraries (umap ${CMAKE_THREAD_LIBS_INIT})

endif(USE_ZFP)


if (USE_ZFP)
endif(USE_ZFP)

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
