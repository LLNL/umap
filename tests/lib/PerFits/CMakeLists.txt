project(umap_PerFits)

add_library(PerFits SHARED PerFits.cpp Tile.cpp )
add_library(PerFits-static STATIC PerFits.cpp Tile.cpp )
set_target_properties(PerFits-static PROPERTIES OUTPUT_NAME PerFits)

add_dependencies(PerFits testoptions)

target_link_libraries(PerFits umap)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS} )

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)

add_custom_command (
  TARGET PerFits
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/PerFits.h ${CMAKE_BINARY_DIR}/include/PerFits.h
)

install(TARGETS PerFits PerFits-static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
        RUNTIME DESTINATION bin )

install(FILES ${CMAKE_BINARY_DIR}/include/PerFits.h DESTINATION include)
