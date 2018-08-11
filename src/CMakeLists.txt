project(umap_libraries)

add_library(libumap SHARED umap.cpp logging/spindle_logc.c logging/spindle_mkdir.c)
add_library(libumap_static STATIC umap.cpp logging/spindle_logc.c logging/spindle_mkdir.c)
set_target_properties(libumap_static PROPERTIES OUTPUT_NAME libumap)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories( 
  BEFORE "${CMAKE_CURRENT_SOURCE_DIR}"
  "${PROJECT_BINARY_DIR}/../include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../include"
  "${CMAKE_SOURCE_DIR}/src/logging"
  "${CMAKE_CURRENT_SOURCE_DIR}/../sysincludes"
  )

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)

add_custom_command (
  TARGET libumap
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../include/umap.h ${CMAKE_BINARY_DIR}/include/umap.h
)

install(TARGETS libumap libumap_static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
        RUNTIME DESTINATION bin )

install(FILES ${CMAKE_BINARY_DIR}/include/umap.h DESTINATION include)

find_package (Threads)
add_executable(umap_logd logging/spindle_logd.cpp)
target_link_libraries (umap_logd ${CMAKE_THREAD_LIBS_INIT})

install(FILES ${PROJECT_BINARY_DIR}/umap_logd 
  DESTINATION libexec
  PERMISSIONS OWNER_EXECUTE OWNER_READ GROUP_EXECUTE GROUP_READ
  )
