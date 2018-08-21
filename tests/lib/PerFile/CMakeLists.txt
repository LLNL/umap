project(umap_PerFile)

add_library(libPerFile SHARED PerFile.cpp)

target_link_libraries(libPerFile libumap)

add_dependencies(libPerFile libtestoptions)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS} )

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)

add_custom_command (
  TARGET libPerFile
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/PerFile.h ${CMAKE_BINARY_DIR}/include/PerFile.h
)

install(TARGETS libPerFile
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
        RUNTIME DESTINATION bin )

install(FILES ${CMAKE_BINARY_DIR}/include/PerFile.h DESTINATION include)
