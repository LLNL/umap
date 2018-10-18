project(umap_testoptions)

add_library(testoptions SHARED testoptions.cpp)
add_library(testoptions-static STATIC testoptions.cpp)
set_target_properties(testoptions-static PROPERTIES OUTPUT_NAME testoptions)

add_dependencies( testoptions umap )

target_link_libraries(testoptions umap)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS} )

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)

add_custom_command (
  TARGET testoptions
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/testoptions.h ${CMAKE_BINARY_DIR}/include/testoptions.h
)

install(TARGETS testoptions testoptions-static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
        RUNTIME DESTINATION bin )

install(FILES ${CMAKE_BINARY_DIR}/include/testoptions.h DESTINATION include)
