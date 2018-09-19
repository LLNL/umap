project(umap_libraries)

add_library(libumap SHARED umap.cpp ../logging/spindle_logc.c ../logging/spindle_mkdir.c)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")


#
# TODO: This needs to change to "AFTER" when we have code in place to
# dynamically check for existence of WP functionality
#
include_directories( AFTER 
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${UMAPINCLUDEDIRS} 
  ${CMAKE_SOURCE_DIR}/sysincludes
)

install(TARGETS libumap
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
        RUNTIME DESTINATION bin )

install(FILES ${CMAKE_SOURCE_DIR}/include/umap.h DESTINATION include)

find_package (Threads)
add_executable(umap_logd ../logging/spindle_logd.cpp)
target_link_libraries (umap_logd ${CMAKE_THREAD_LIBS_INIT})

install(FILES ${PROJECT_BINARY_DIR}/umap_logd 
  DESTINATION libexec
  PERMISSIONS OWNER_EXECUTE OWNER_READ GROUP_EXECUTE GROUP_READ
)
