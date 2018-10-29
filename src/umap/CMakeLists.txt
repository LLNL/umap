project(umap_libraries)
set(umapsrc 
    umap.cpp 
    ../logging/spindle_logc.c 
    ../logging/spindle_mkdir.c
    ../store/Store.cpp
    ../store/StoreFile.cpp
)
add_library(umap SHARED ${umapsrc} )
add_library(umap-static STATIC ${umapsrc} )
set_target_properties(umap-static PROPERTIES OUTPUT_NAME umap)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories( AFTER 
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${UMAPINCLUDEDIRS} 
)

install(TARGETS umap umap-static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
        RUNTIME DESTINATION bin )

install(FILES ${CMAKE_SOURCE_DIR}/src/include/umap.h DESTINATION include)

find_package (Threads)
add_executable(umap_logd ../logging/spindle_logd.cpp)
target_link_libraries (umap_logd ${CMAKE_THREAD_LIBS_INIT})

install(FILES ${PROJECT_BINARY_DIR}/umap_logd 
  DESTINATION libexec
  PERMISSIONS OWNER_EXECUTE OWNER_READ GROUP_EXECUTE GROUP_READ
)
