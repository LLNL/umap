cmake_minimum_required (VERSION 3.5.1)
project(umap
  VERSION 0.0.3
  LANGUAGES CXX C
)

OPTION (ENABLE_TESTS "Selects whether tests are built." On)
OPTION (ENABLE_LOGGING "Build umap with Logging enabled" On)
OPTION (ENABLE_CFITS_TESTS "Build CFITS-based Tests (requires cfits library)" Off)

include(cmake/BuildEnv.cmake)
include(cmake/BuildType.cmake)
include(cmake/SetupUmapThirdParty.cmake)

set(UMAP_DEBUG_LOGGING ${ENABLE_LOGGING})
configure_file(
  ${PROJECT_SOURCE_DIR}/config/config.h.in
  ${PROJECT_BINARY_DIR}/src/include/config.h)

set (UMAPINCLUDEDIRS
    "${CMAKE_BINARY_DIR}/src/include"
    "${CMAKE_SOURCE_DIR}/src/include"
    "${CMAKE_SOURCE_DIR}/src/logging"
    "${CMAKE_SOURCE_DIR}/src/store"
    )

add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(tests)

