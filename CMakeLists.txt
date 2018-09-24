cmake_minimum_required (VERSION 3.5.1)
project(umap
  VERSION 0.0.3
  LANGUAGES CXX C
)

OPTION (ENABLE_CFITS "Build umap with Logging enabled" Off)
OPTION (ENABLE_FITS_TESTS "Build FITS-based Tests (requires qfits library)" Off)
OPTION (ENABLE_TESTS "Selects whether tests are built." On)
OPTION (ENABLE_LOGGING "Build umap with Logging enabled" On)
OPTION (ENABLE_STATS "Enable display statistics on exit" Off)

include(cmake/BuildEnv.cmake)
include(cmake/BuildType.cmake)
include(cmake/SetupUmapThirdParty.cmake)

set(UMAP_ENABLE_LOGGING ${ENABLE_LOGGING})
set(UMAP_ENABLE_CFITS ${ENABLE_CFITS})
set(UMAP_DISPLAY_STATS ${ENABLE_STATS})

configure_file(
  ${PROJECT_SOURCE_DIR}/config/config.h.in
  ${PROJECT_BINARY_DIR}/include/config.h)

set (UMAPINCLUDEDIRS
    "${CMAKE_BINARY_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src/logging"
    "${CMAKE_SOURCE_DIR}/tests/lib/options"
    "${CMAKE_SOURCE_DIR}/tests/lib/PerFile"
    "${CMAKE_SOURCE_DIR}/tests/lib/PerFits"
    )

add_subdirectory(src)

if ( ENABLE_TESTS )
  add_subdirectory(tests)
endif()

