add_subdirectory(options)

if ( ENABLE_FITS_TESTS )
  add_subdirectory(PerFits)
endif()

add_subdirectory(PerFile)
