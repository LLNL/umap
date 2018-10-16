if ( ENABLE_CFITS )
  FIND_PACKAGE(CURL)
  if ( NOT CURL_FOUND )
    message ( STATUS "CURL library not found, will attempt to use cfitsio without CURL support")
  endif()

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
endif()
