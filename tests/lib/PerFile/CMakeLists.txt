project(umap_PerFile)

add_library(libPerFile SHARED PerFile.cpp)
add_library(libPerFile_static STATIC PerFile.cpp)

add_dependencies(libPerFile libtestoptions)
add_dependencies(libPerFile_static libtestoptions_static)

target_link_libraries(libPerFile libumap)
target_link_libraries(libPerFile_static libumap_static)

set_target_properties(libPerFile_static PROPERTIES OUTPUT_NAME libPerFile)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include )

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)

add_custom_command (
  TARGET libPerFile
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/PerFile.h ${CMAKE_BINARY_DIR}/include/PerFile.h
)

install(TARGETS libPerFile libPerFile_static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
        RUNTIME DESTINATION bin )

install(FILES ${CMAKE_BINARY_DIR}/include/PerFile.h DESTINATION include)
