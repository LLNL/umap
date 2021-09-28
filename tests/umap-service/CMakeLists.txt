#############################################################################
# Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################
project(umap-server)

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS 
    "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
  add_executable(umap-server umap-server.cpp)
  add_executable(singlefile-no-unmap singlefile-no-unmap.cpp)
  add_executable(diff_mpumap_app diff_mpumap_app.cpp)

  if(STATIC_UMAP_LINK)
     set(umap-lib "mpumapd-static")
     set(umapclient-lib "mpumapclient-static")
  else()
     set(umap-lib "mpumapd")
     set(umapclient-lib "mpumapclient")
  endif()
 
  add_dependencies(umap-server ${umap-lib})
  target_link_libraries(umap-server ${umap-lib}) 
  add_dependencies(singlefile-no-unmap ${umapclient-lib})
  target_link_libraries(singlefile-no-unmap ${umapclient-lib}) 
  add_dependencies(diff_mpumap_app ${umapclient-lib})
  target_link_libraries(diff_mpumap_app ${umapclient-lib}) 

  install(TARGETS umap-server singlefile-no-unmap diff_mpumap_app 
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
    RUNTIME DESTINATION bin )
else()
  message("Skipping flush_buffer, OpenMP required")
endif()
