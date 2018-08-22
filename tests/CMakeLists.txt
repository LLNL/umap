add_subdirectory(lib)

if ( ENABLE_FITS_TESTS )
  add_subdirectory(median_calculation)
endif()

add_subdirectory(churn)
add_subdirectory(pfbenchmark)
add_subdirectory(umapcpu)
add_subdirectory(umapsort)
add_subdirectory(bfs)
