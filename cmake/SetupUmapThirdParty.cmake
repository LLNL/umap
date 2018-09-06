if ( ENABLE_CFITS )
  find_library( T curl )
  if ( T )
    set (CFITS_CURL_LIB "curl")
  endif()

  find_library( T nsl )
  if ( T )
    set (CFITS_NSL_LIB "nsl")
  endif()

  find_library( T m )
  if ( T )
    set (CFITS_M_LIB "m")
  endif()

  find_library( CFITS_LIBRARY
    libcfitsio.a
    PATHS ${CFITS_LIBRARY_PATH}
  )

  if ( NOT CFITS_LIBRARY )
    message( FATAL_ERROR "Could not find CFITS library, make sure CFITS_LIBRARY_PATH is set properly")
  endif()

  find_path( CFITS_INCLUDE_DIR
    fitsio.h
    PATHS ${CFITS_INCLUDE_PATH}
  )

  if ( NOT CFITS_INCLUDE_DIR )
    message(FATAL_ERROR "Could not find CFITS include directory, make sure CFITS_INCLUDE_PATH is set properly")
  endif()

  include_directories( ${CFITS_INCLUDE_DIR} )

endif()
