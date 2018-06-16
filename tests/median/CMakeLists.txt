project(median)


FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    add_executable(median median.cpp)
    add_executable(median_cube median_cube.cpp)

    target_link_libraries(median libumap_static)
    target_link_libraries(median libtestoptions_static)
    target_link_libraries(median libPerFile_static)
    target_link_libraries(median_cube libumap_static)
    target_link_libraries(median_cube libtestoptions_static)
    target_link_libraries(median_cube libPerFile_static)

    include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include )

    install(TARGETS median median_cube
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )
else()
  message("Skipping median, OpenMP required")
endif()
