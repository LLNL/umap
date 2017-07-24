cmake_minimum_required (VERSION 3.5.2)
project(umap VERSION 0.0.1)

if (NOT CMAKE_BUILD_TYPE)
  message(STATUS "No build type selected, default to Debug")
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (default Debug)" FORCE)
endif()

set (FLAGS_ALL "-Wall")
set (FLAGS_DEBUG_ALL "-g -O0 -DDEBUG")
set (FLAGS_RELEASE_ALL "-O3")

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 ${FLAGS_ALL}")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ${FLAGS_ALL}")
set (CMAKE_CXX_FLAGS_DEBUG "${FLAGS_DEBUG_ALL}")
set (CMAKE_C_FLAGS_DEBUG "${FLAGS_DEBUG_ALL}")
set (CMAKE_CXX_FLAGS_RELEASE "${FLAGS_RELEASE_ALL}")
set (CMAKE_C_FLAGS_RELEASE "${FLAGS_RELEASE_ALL}")

add_subdirectory(src)

OPTION (ENABLE_TESTS "Selects whether tests are built." TRUE)
if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()

