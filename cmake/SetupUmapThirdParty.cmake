if ( ENABLE_CFITS )
  FIND_PACKAGE(CURL)

  find_library( CFITS_LIBRARY libcfitsio.a PATHS ${CFITS_LIBRARY_PATH} )
  if ( NOT CFITS_LIBRARY )
    message( FATAL_ERROR "Could not find CFITS library, make sure CFITS_LIBRARY_PATH is set properly")
  endif()

  find_library( BZ2_LIBRARY libbz2.so PATHS ${CFITS_LIBRARY_PATH} )
  find_library( ZLIB_LIBRARY libz.so PATHS ${CFITS_LIBRARY_PATH} )
  set(CFITS_LIBRARIES ${CFITS_LIBRARY} ${BZ2_LIBRARY} ${ZLIB_LIBRARY})

  find_path( CFITS_INCLUDE_DIR fitsio.h PATHS ${CFITS_INCLUDE_PATH} )
  if ( NOT CFITS_INCLUDE_DIR )
    message(FATAL_ERROR "Could not find CFITS include directory, make sure CFITS_INCLUDE_PATH is set properly")
  endif()

  include_directories( ${CFITS_INCLUDE_DIR} )
endif()
