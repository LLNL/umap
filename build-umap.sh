#!/bin/bash
mkdir install; mkdir build; cd build;
cmake3 -DCMAKE_BUILD_PREFIX=. -DENABLE_TESTS_LINK_STATIC_UMAP=ON -DCMAKE_INSTALL_PREFIX=../install ..; cmake3 --build . --target install
#cmake3 -DCMAKE_BUILD_PREFIX=. -DCMAKE_INSTALL_PREFIX=../install ..; cmake3 --build . --target install
