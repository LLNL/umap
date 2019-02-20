#############################################################################
# Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################
project(umap_libraries)
set(umapsrc
    umap.cpp
    ../store/Store.cpp
    ../store/StoreFile.cpp
)

if ( ENABLE_LOGGING )
  set(umapsrc ${umapsrc}
    ../logging/spindle_logc.c
    ../logging/spindle_mkdir.c
  )
endif()

find_package(Threads REQUIRED)
add_library(umap SHARED ${umapsrc} )
add_library(umap-static STATIC ${umapsrc} )
set_target_properties(umap-static PROPERTIES OUTPUT_NAME umap)
target_link_libraries (umap ${CMAKE_THREAD_LIBS_INIT})

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories( AFTER
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${UMAPINCLUDEDIRS}
)

install(TARGETS umap umap-static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin )

install(FILES
  ${CMAKE_SOURCE_DIR}/src/include/umap/umap.h
  ${CMAKE_SOURCE_DIR}/src/include/umap/Store.h
  DESTINATION include/umap)

if ( ENABLE_LOGGING )
  add_executable(umap_logd ../logging/spindle_logd.cpp)
  target_link_libraries (umap_logd ${CMAKE_THREAD_LIBS_INIT})

  install(FILES ${PROJECT_BINARY_DIR}/umap_logd
    DESTINATION libexec
    PERMISSIONS OWNER_EXECUTE OWNER_READ GROUP_EXECUTE GROUP_READ
  )
endif()
