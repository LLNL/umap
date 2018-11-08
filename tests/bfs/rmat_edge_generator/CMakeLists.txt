set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

add_executable(generate_edge_list generate_edge_list.cpp)
install(TARGETS generate_edge_list
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
        RUNTIME DESTINATION bin )