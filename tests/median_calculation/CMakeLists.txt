project(median_calculation)

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")

    include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include )

    add_executable(test_median_calculation test_median_calculation.cpp)
    add_dependencies(test_median_calculation libtestoptions_static libumap_static libPerFile_static libPerFits_static )

    install(TARGETS test_median_calculation
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )

    add_executable(random_vector_median_calculation random_vector_median_calculation.cpp)

    add_dependencies(random_vector_median_calculation libumap_static libtestoptions_static libPerFits_static )

    target_link_libraries(random_vector_median_calculation libumap_static libtestoptions_static libPerFits_static ${CFITS_LIBRARY} curl )
    target_link_libraries(test_median_calculation libumap_static libtestoptions_static libPerFits_static ${CFITS_LIBRARY} curl )

    install(TARGETS random_vector_median_calculation
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )

else()
  message("Skipping median_calculation, OpenMP required")
endif()


