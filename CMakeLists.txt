cmake_minimum_required (VERSION 3.5.1)
project(umap 
  VERSION 0.0.2
  LANGUAGES CXX C
)

OPTION (ENABLE_CFITS "Build umap with Logging enabled" Off)
OPTION (ENABLE_FITS_TESTS "Build FITS-based Tests (requires qfits library)" Off)
OPTION (ENABLE_TESTS "Selects whether tests are built." On)
OPTION (ENABLE_LOGGING "Build umap with Logging enabled" On)

include(cmake/BuildEnv.cmake)
include(cmake/BuildType.cmake)
include(cmake/SetupUmapThirdParty.cmake)

set(UMAP_ENABLE_LOGGING ${ENABLE_LOGGING})
set(UMAP_ENABLE_CFITS ${ENABLE_CFITS})

configure_file(
  ${PROJECT_SOURCE_DIR}/config/config.h.in
  ${PROJECT_BINARY_DIR}/include/config.h)

add_subdirectory(src)

if ( ENABLE_TESTS )
  add_subdirectory(tests)
endif()

