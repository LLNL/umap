project(umapsort)

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/run_umapsort.sh"
    "${CMAKE_CURRENT_BINARY_DIR}/run_umapsort.sh"
    COPYONLY
  )

  include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS} )

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
  
  add_executable(umapsort main.cpp)

  set(CMAKE_SKIP_RPATH TRUE)
  
  if(STATIC_UMAP_LINK)
    set(umap-lib "umap-static")
  else()
    set(umap-lib "umap")
  endif()
    
  add_dependencies(umapsort ${umap-lib})

  target_link_libraries(umapsort ${umap-lib})

  install(TARGETS umapsort
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
    RUNTIME DESTINATION bin )
else()
  message("Skipping umapsort, OpenMP required")
endif()

