project(pfbenchmark)

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")

    add_executable(pfbenchmark-read pfbenchmark.cpp)
    add_executable(pfbenchmark-write pfbenchmark.cpp)
    add_executable(pfbenchmark-readmodifywrite pfbenchmark.cpp)
    add_executable(nvmebenchmark-read nvmebenchmark.cpp)
    add_executable(nvmebenchmark-write nvmebenchmark.cpp)

    target_link_libraries(pfbenchmark-read libumap_static libtestoptions_static libPerFile_static)
    target_link_libraries(pfbenchmark-write libumap_static libtestoptions_static libPerFile_static)
    target_link_libraries(pfbenchmark-readmodifywrite libumap_static libtestoptions_static libPerFile_static)
    target_link_libraries(nvmebenchmark-read libumap_static libtestoptions_static libPerFile_static)
    target_link_libraries(nvmebenchmark-write libumap_static libtestoptions_static libPerFile_static)

    include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include )

    install(TARGETS pfbenchmark-read pfbenchmark-write pfbenchmark-readmodifywrite nvmebenchmark-read nvmebenchmark-write
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )
else()
  message("Skipping pfbenchmark, OpenMP required")
endif()


