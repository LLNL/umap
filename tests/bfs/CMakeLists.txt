project(bfs)

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")

    include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include )

    add_executable(ingest_edge_list ingest_edge_list.cpp)
    target_link_libraries(ingest_edge_list)
    target_link_libraries(ingest_edge_list)
    target_link_libraries(ingest_edge_list)
    target_link_libraries(ingest_edge_list)
    install(TARGETS ingest_edge_list
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )

    add_executable(run_bfs run_bfs.cpp)
    target_link_libraries(run_bfs)
    target_link_libraries(run_bfs)
    target_link_libraries(run_bfs)
    target_link_libraries(run_bfs)
    install(TARGETS run_bfs
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )

    add_executable(test_bfs test_bfs.cpp)
    target_link_libraries(test_bfs)
    target_link_libraries(test_bfs)
    target_link_libraries(test_bfs)
    target_link_libraries(test_bfs)
    install(TARGETS test_bfs
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )

else()
  message("Skipping bfs, OpenMP required")
endif()


