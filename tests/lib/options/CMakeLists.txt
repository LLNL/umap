project(umap_testoptions)

add_library(libtestoptions SHARED testoptions.cpp)
add_library(libtestoptions_static STATIC testoptions.cpp)

add_dependencies( libtestoptions libumap libumap_static )
add_dependencies( libtestoptions_static libumap libumap_static )

target_link_libraries(libtestoptions libumap)
target_link_libraries(libtestoptions_static libumap_static)

set_target_properties(libtestoptions_static PROPERTIES OUTPUT_NAME libtestoptions)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include )

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)

add_custom_command (
  TARGET libtestoptions
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/testoptions.h ${CMAKE_BINARY_DIR}/include/testoptions.h
)

install(TARGETS libtestoptions libtestoptions_static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
        RUNTIME DESTINATION bin )

install(FILES ${CMAKE_BINARY_DIR}/include/testoptions.h DESTINATION include)
