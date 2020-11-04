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

if (MARGO_ROOT)
    set(networkheaders
	store/StoreNetwork.h
	store/rpc/rpc_server.hpp
	store/rpc/rpc_client.hpp
	store/rpc/rpc_util.hpp
	)
    set(networksrcs
	store/StoreNetwork.cpp
	store/rpc/rpc_server.cpp
	store/rpc/rpc_client.cpp
    	)
    list(APPEND umapheaders ${networkheaders})
    list(APPEND umapsrc ${networkheaders} ${networksrcs})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMARGO_ROOT")
    
    find_package(MPI REQUIRED)
    include_directories(${MPI_INCLUDE_PATH})
    include_directories(${MARGO_ROOT}/include)
    find_library(LIBMARGO NAMES margo PATHS ${MARGO_ROOT}/lib)
    if (LIBMARGO)
       link_libraries(${LIBMARGO})
    else()
       message(FATAL_ERROR "Cannot find MARGO library")
    endif()
endif()


add_library(umap SHARED ${umapsrc} )
add_library(umap-static STATIC ${umapsrc} )
set_target_properties(umap-static PROPERTIES OUTPUT_NAME umap)
target_link_libraries (umap ${CMAKE_THREAD_LIBS_INIT} ${MPI_LIBRARIES} ${MARGO_LIBRARIES})

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

install(TARGETS umap umap-static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin )

install(FILES umap.h DESTINATION include/umap)

install(FILES store/Store.hpp DESTINATION include/umap/store )

install(FILES store/SparseStore.h DESTINATION include/umap/store)

install(FILES store/StoreNetwork.h DESTINATION include/umap/store )
