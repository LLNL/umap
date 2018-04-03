project(umapsort)

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/perftest_mmap.sh"
      "${CMAKE_CURRENT_BINARY_DIR}/perftest_mmap.sh"
      COPYONLY
      )
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/perftest_umap.sh"
      "${CMAKE_CURRENT_BINARY_DIR}/perftest_umap.sh"
      COPYONLY
      )
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    add_executable(umapsort umapsort.cpp)

    target_link_libraries(umapsort libumap_static)
    target_link_libraries(umapsort libtestoptions_static)
    target_link_libraries(umapsort libPerFile_static)

    include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include )

    install(TARGETS umapsort
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )
else()
    message("Skpping umapsort, OpenMP required")
endif()


