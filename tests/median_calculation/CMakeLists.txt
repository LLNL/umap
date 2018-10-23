project(median_calculation)

FIND_PACKAGE(CURL QUIET)

find_library( CFITS_LIBRARY libcfitsio.a PATHS ${CFITS_LIBRARY_PATH} )
if ( NOT CFITS_LIBRARY )
  message( FATAL_ERROR "Could not find CFITS library, make sure CFITS_LIBRARY_PATH is set properly")
endif()

set(CFITS_LIBRARIES ${CFITS_LIBRARY} ${CURL_LIBRARIES} )

find_path( CFITS_INCLUDE_DIR fitsio.h PATHS ${CFITS_INCLUDE_PATH} )
if ( NOT CFITS_INCLUDE_DIR )
  message(FATAL_ERROR "Could not find CFITS include directory, make sure CFITS_INCLUDE_PATH is set properly")
endif()

include_directories( ${CFITS_INCLUDE_DIR} )

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")

    include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS})

    add_executable(test_median_calculation test_median_calculation.cpp)
    add_dependencies(test_median_calculation umap)
    target_link_libraries(test_median_calculation umap ${CFITS_LIBRARIES})
    install(TARGETS test_median_calculation
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )

    add_executable(run_random_vector run_random_vector.cpp)
    add_dependencies(run_random_vector umap)
    target_link_libraries(run_random_vector umap ${CFITS_LIBRARIES})
    install(TARGETS run_random_vector
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )

else()
  message("Skipping median_calculation, OpenMP required")
endif()


