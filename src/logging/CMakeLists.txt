project(logging)

find_package (Threads)
add_executable(umap_logd spindle_logd.cpp)
target_link_libraries (umap_logd ${CMAKE_THREAD_LIBS_INIT})

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include )

add_library(umap_logging SHARED spindle_logc.c spindle_mkdir.c)
add_library(umap_logging_static STATIC spindle_logc.c spindle_mkdir.c)

set_target_properties(umap_logging_static PROPERTIES OUTPUT_NAME umap_logging)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include )

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)

install(TARGETS umap_logging umap_logging_static
      LIBRARY DESTINATION lib
      ARCHIVE DESTINATION lib/static
      RUNTIME DESTINATION bin )

install(FILES ${PROJECT_BINARY_DIR}/umap_logd 
  DESTINATION libexec
  PERMISSIONS OWNER_EXECUTE OWNER_READ GROUP_EXECUTE GROUP_READ
  )
