project(umap_testlibraries)

add_library(libumaptest SHARED mmap.cpp options.cpp)
add_library(libumaptest_static STATIC mmap.cpp options.cpp)

target_link_libraries(libumaptest libumap)
target_link_libraries(libumaptest_static libumap_static)

set_target_properties(libumaptest_static PROPERTIES OUTPUT_NAME libumaptest)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include )

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)

add_custom_command (
  TARGET libumaptest
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/umaptest.h ${CMAKE_BINARY_DIR}/include/umaptest.h
)

install(TARGETS libumaptest libumaptest_static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
        RUNTIME DESTINATION bin )

install(FILES ${CMAKE_BINARY_DIR}/include/umaptest.h DESTINATION include)
