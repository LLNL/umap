project(FITS)

FIND_PACKAGE( OpenMP REQUIRED )
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    add_executable(readxyz readxyz.cpp)
    add_executable(multiple multiple.cpp)
    add_executable(simple simple_loader.cpp)
    
    target_link_libraries(multiple libumap_static)
    target_link_libraries(multiple libtestoptions_static)
    target_link_libraries(multiple libPerFits_static)
    target_link_libraries(multiple ${CFITS_LIBRARY} )

    target_link_libraries(readxyz libumap_static)
    target_link_libraries(readxyz libtestoptions_static)
    target_link_libraries(readxyz libPerFits_static)
    target_link_libraries(readxyz ${CFITS_LIBRARY} )

    target_link_libraries(simple libtestoptions_static)

    include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include )

    install(TARGETS multiple simple readxyz
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static
            RUNTIME DESTINATION bin )

else()
  message("Skipping median, OpenMP required")
endif()
