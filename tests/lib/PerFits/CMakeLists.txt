project(umap_PerFits)

add_library(libPerFits SHARED PerFits.cpp Tile.cpp )

add_dependencies(libPerFits libtestoptions)

target_link_libraries(libPerFits libumap)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS} )

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)

add_custom_command (
  TARGET libPerFits
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/PerFits.h ${CMAKE_BINARY_DIR}/include/PerFits.h
)

install(TARGETS libPerFits libPerFits
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
        RUNTIME DESTINATION bin )

install(FILES ${CMAKE_BINARY_DIR}/include/PerFits.h DESTINATION include)
