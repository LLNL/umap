if ( ENABLE_CFITS )
  FIND_PACKAGE(CURL)

  find_library( CFITS_LIBRARIES libcfitsio.a PATHS ${CFITS_LIBRARY_PATH} )
  if ( NOT CFITS_LIBRARIES )
    message( FATAL_ERROR "Could not find CFITS library, make sure CFITS_LIBRARY_PATH is set properly")
  endif()

  find_path( CFITS_INCLUDE_DIR fitsio.h PATHS ${CFITS_INCLUDE_PATH} )
  if ( NOT CFITS_INCLUDE_DIR )
    message(FATAL_ERROR "Could not find CFITS include directory, make sure CFITS_INCLUDE_PATH is set properly")
  endif()

  include_directories( ${CFITS_INCLUDE_DIR} )
endif()
