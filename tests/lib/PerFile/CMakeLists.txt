project(umap_PerFile)

add_library(PerFile SHARED PerFile.cpp)
add_library(PerFile-static STATIC PerFile.cpp)
set_target_properties(PerFile-static PROPERTIES OUTPUT_NAME PerFile)

target_link_libraries(PerFile umap)

add_dependencies(PerFile testoptions)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS} )

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)

add_custom_command (
  TARGET PerFile
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/PerFile.h ${CMAKE_BINARY_DIR}/include/PerFile.h
)

install(TARGETS PerFile PerFile-static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
        RUNTIME DESTINATION bin )

install(FILES ${CMAKE_BINARY_DIR}/include/PerFile.h DESTINATION include)
