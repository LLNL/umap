/*
This file is part of UMAP.  For copyright information see the COPYRIGHT
file in the top level directory, or at
https://github.com/LLNL/umap/blob/master/COPYRIGHT
This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free
Software Foundation) version 2.1 dated February 1999.  This program is
distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the terms and conditions of the GNU Lesser General Public License
for more details.  You should have received a copy of the GNU Lesser General
Public License along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
#ifndef UMAP_APPS_UTILITY_OPEN_MP_HPP
#define UMAP_APPS_UTILITY_OPEN_MP_HPP

#include <string>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace utility {

inline std::string omp_schedule_kind_name(const int kind_in_int) {
#ifdef _OPENMP
  if (kind_in_int == omp_sched_static) {
    return std::string("omp_sched_static (" + std::to_string(kind_in_int) + ")");
  } else if (kind_in_int == omp_sched_dynamic) {
    return std::string("omp_sched_dynamic (" + std::to_string(kind_in_int) + ")");
  } else if (kind_in_int == omp_sched_guided) {
    return std::string("omp_sched_guided (" + std::to_string(kind_in_int) + ")");
  } else if (kind_in_int == omp_sched_auto) {
    return std::string("omp_sched_auto (" + std::to_string(kind_in_int) + ")");
  }
  return std::string("Unknown kind (" + std::to_string(kind_in_int) + ")");
#else
  return std::string("OpenMP is not supported");
#endif
};

} // namespace utility
#endif //UMAP_APPS_UTILITY_OPEN_MP_HPP
