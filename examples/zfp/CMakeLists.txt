project(poisson)

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/run.sh"
      "${CMAKE_CURRENT_BINARY_DIR}/run.sh"
      COPYONLY
    )

    include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS} ${CMAKE_BINARY_DIR}/zfp/src/ZFP/include ${CMAKE_BINARY_DIR}/zfp/src/ZFP/include/zfp)

    add_executable(poisson poisson.cpp)

    set(CMAKE_SKIP_RPATH TRUE)
  
    if(STATIC_UMAP_LINK)
      set(umap-lib "umap-static")
    else()
      set(umap-lib "umap")
    endif()
  
    add_dependencies(poisson ${umap-lib})
    target_link_libraries(poisson ${umap-lib})

    install(TARGETS bfs
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )

else()
  message("Skipping zfp poisson, OpenMP required")
endif()
