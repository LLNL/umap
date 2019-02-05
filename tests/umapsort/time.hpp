//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory
//
// Created by Marty McFadden, 'mcfadden8 at llnl dot gov'
// LLNL-CODE-733797
//
// All rights reserved.
//
// This file is part of UMAP.
//
// For details, see https://github.com/LLNL/umap
// Please also see the COPYRIGHT and LICENSE files for LGPL license.
//////////////////////////////////////////////////////////////////////////////

#ifndef UMAP_TEST_LIB_UTILITY_TIME_HPP
#define UMAP_TEST_LIB_UTILITY_TIME_HPP

#include <chrono>

namespace utility {
inline std::chrono::high_resolution_clock::time_point elapsed_time_sec() {
  return std::chrono::high_resolution_clock::now();
}

inline double elapsed_time_sec(const std::chrono::high_resolution_clock::time_point &tic) {
  auto duration_time = std::chrono::high_resolution_clock::now() - tic;
  return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(duration_time).count() / 1e6);
}
}
#endif //UMAP_TEST_LIB_UTILITY_TIME_HPP
