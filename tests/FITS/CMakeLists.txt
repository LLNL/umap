project(FITS)

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    file(GLOB UTIL_HPP_FILES ../utility "*.hpp")

    add_executable(exdata_fits exdata_fits.c)
    add_executable(generate_dummy_cube generate_dummy_cube.cpp median_calculation_kernel.hpp ${UTIL_HPP_FILES})
    add_executable(median_calculation median_calculation.cpp median_calculation_kernel.hpp ${UTIL_HPP_FILES})

    target_link_libraries(median_calculation libumap_static)
    target_link_libraries(median_calculation libumaptest_static)

    include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include )

    install(TARGETS exdata_fits generate_dummy_cube median_calculation
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )

else()
    message("Skpping FITS, OpenMP required")
endif()
