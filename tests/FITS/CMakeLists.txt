project(FITS)

if (BUILD_QFITS)
    add_library(qfits SHARED IMPORTED)
    set_target_properties(qfits PROPERTIES
            IMPORTED_LOCATION "$ENV{HOME}/qfits/local/lib/libqfits.so.0.0.0"
            INTERFACE_INCLUDE_DIRECTORIES "$ENV{HOME}/local/qfits/include")
endif()

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    file(GLOB UTIL_HPP_FILES ../utility "*.hpp")
#    add_executable(multiple multiple.cpp)
#    add_executable(simple simple_loader.cpp)
#    add_executable(private multi_private.cpp)
    add_executable(generate_dummy_cube generate_dummy_cube.cpp median_calculation_kernel.hpp ${UTIL_HPP_FILES})
    add_executable(median_calculation median_calculation.cpp median_calculation_kernel.hpp ${UTIL_HPP_FILES})

    target_link_libraries(median_calculation libumap_static libumaptest_static)
#    target_link_libraries(multiple libumap_static)
#    target_link_libraries(multiple libumaptest_static)
#    target_link_libraries(multiple qfits)

#    target_link_libraries(simple libumaptest_static)
#    target_link_libraries(simple qfits)
#
#    target_link_libraries(private libumap_static)
#    target_link_libraries(private libumaptest_static)
#    target_link_libraries(private qfits)

    include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include )
    install(TARGETS generate_dummy_cube median_calculation
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )

else()
    message("Skpping FITS, OpenMP required")
endif()
