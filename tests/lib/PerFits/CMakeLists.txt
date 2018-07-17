project(umap_PerFits)

add_library(libPerFits SHARED PerFits.cpp Tile.cpp )
add_library(libPerFits_static STATIC PerFits.cpp Tile.cpp )

add_dependencies(libPerFits libtestoptions)
add_dependencies(libPerFits_static libtestoptions_static)

target_link_libraries(libPerFits libumap)
target_link_libraries(libPerFits_static libumap_static)

set_target_properties(libPerFits_static PROPERTIES OUTPUT_NAME libPerFits)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include )

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)

add_custom_command (
  TARGET libPerFits_static
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/PerFits.h ${CMAKE_BINARY_DIR}/include/PerFits.h
)

add_custom_command (
  TARGET libPerFits
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/PerFits.h ${CMAKE_BINARY_DIR}/include/PerFits.h
)

install(TARGETS libPerFits libPerFits_static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
        RUNTIME DESTINATION bin )

install(FILES ${CMAKE_BINARY_DIR}/include/PerFits.h DESTINATION include)
