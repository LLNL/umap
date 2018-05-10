add_subdirectory(lib)

if ( ENABLE_FITS_TESTS )
  add_subdirectory(FITS)
  add_subdirectory(median_calculation)
endif()

add_subdirectory(umapsort)
add_subdirectory(umapcpu)
add_subdirectory(umapmillions)
add_subdirectory(median)
add_subdirectory(readload)
add_subdirectory(churn)
add_subdirectory(rwseq)
