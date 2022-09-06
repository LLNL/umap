project(bfs)

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/run_bfs.sh"
      "${CMAKE_CURRENT_BINARY_DIR}/run_bfs.sh"
      COPYONLY
    )

    include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${UMAPINCLUDEDIRS} )

    add_executable(bfs main.cpp)

    set(CMAKE_SKIP_RPATH TRUE)
  
    if(STATIC_UMAP_LINK)
      set(umap-lib "umap-static")
    else()
      set(umap-lib "umap")
    endif()
  
    add_dependencies(bfs ${umap-lib})
    target_link_libraries(bfs ${umap-lib})

    install(TARGETS bfs
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )

else()
  message("Skipping bfs, OpenMP required")
endif()

add_subdirectory(rmat_graph_generator)
