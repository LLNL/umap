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

#ifndef UTILITY_OPENMP_HPP
#define UTILITY_OPENMP_HPP

#ifdef _OPENMP
#include <omp.h>
#endif

namespace utility
{

size_t openmp_get_thread_num()
{
#ifdef _OPENMP
  return omp_get_thread_num();
#else
  return 0;
#endif
}

size_t openmp_get_num_threads()
{
#ifdef _OPENMP
  return omp_get_num_threads();
#else
  return 1;
#endif
}

int openmp_set_num_threads(const int num_threads)
{
  int configured_num_threads = 0;
#ifdef _OPENMP
  if (num_threads > 0)
    omp_set_num_threads(num_threads);

#pragma omp parallel
  {
    if(omp_get_thread_num() == 0) {
      configured_num_threads = omp_get_num_threads();
      std::cout << "Run with " << omp_get_num_threads() << " threads" << std::endl;
    }
  }
#else
  std::cout << "Run without OpenMP" << std::endl;
#endif

  return configured_num_threads;
}


} // namespace utility
#endif // UTILITY_OPENMP_HPP