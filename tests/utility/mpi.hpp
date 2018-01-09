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

#ifndef UTILITY_MPI_HPP
#define UTILITY_MPI_HPP

#include <iostream>
#include <string>
#include "mpi.h"

namespace utility
{

inline void mpi_abort(const MPI_Comm comm = MPI_COMM_WORLD)
{
  MPI_Abort(MPI_COMM_WORLD, 1);
}

inline void mpi_check_error(const int error_value, const MPI_Comm comm = MPI_COMM_WORLD)
{
  if (error_value == MPI_SUCCESS)
    return;

  char *error_string = nullptr;
  int len = 0;
  MPI_Error_string(error_value, error_string, &len);
  std::cout << " MPI error = " << error_string << std::endl;
  mpi_abort(MPI_COMM_WORLD);
}

inline void mpi_init(int argc, char *argv[])
{
  mpi_check_error(MPI_Init(&argc, &argv));
}

inline int get_mpi_comm_size(const MPI_Comm comm = MPI_COMM_WORLD)
{
  int size = 0;
  mpi_check_error(MPI_Comm_size(comm, &size));
  return size;
}

int get_mpi_comm_rank(const MPI_Comm comm = MPI_COMM_WORLD)
{
  int rank = -1;
  mpi_check_error(MPI_Comm_rank(comm, &rank));
  return rank;
}

inline void mpi_barrier(const MPI_Comm comm = MPI_COMM_WORLD)
{
  mpi_check_error(MPI_Barrier(comm));
}

inline void mpi_finalize()
{
  mpi_check_error(MPI_Finalize());
}

void mpi_reduce(const void* const send_buf,
                void* const recv_buf,
                const int count,
                const MPI_Datatype datatype,
                const MPI_Op op,
                const int root,
                const MPI_Comm communicator = MPI_COMM_WORLD)
{
  mpi_check_error(MPI_Reduce(send_buf,
                             recv_buf,
                             count,
                             datatype,
                             op,
                             root,
                             communicator));
}

}  // namespace utility
#endif // UTILITY_MPI_HPP
