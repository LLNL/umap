project(umap_testoptions)

add_library(libtestoptions SHARED testoptions.cpp)

add_dependencies( libtestoptions libumap )

target_link_libraries(libtestoptions libumap)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS} )

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)

add_custom_command (
  TARGET libtestoptions
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/testoptions.h ${CMAKE_BINARY_DIR}/include/testoptions.h
)

install(TARGETS libtestoptions
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
        RUNTIME DESTINATION bin )

install(FILES ${CMAKE_BINARY_DIR}/include/testoptions.h DESTINATION include)
