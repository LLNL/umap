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
/*
 * Copyright (c) 2013, Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory.
 * Written by Roger Pearce <pearce7@llnl.gov>.
 * LLNL-CODE-624712.
 * All rights reserved.
 *
 * This file is part of LRIOT, Version 1.0.
 * For details, see https://computation.llnl.gov/casc/dcca-pub/dcca/Downloads.html
 *
 * Please also read this link – Additional BSD Notice.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * • Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the disclaimer below.
 *
 * • Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the disclaimer (as noted below) in the
 *   documentation and/or other materials provided with the distribution.
 *
 * • Neither the name of the LLNS/LLNL nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY, LLC,
 * THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Additional BSD Notice
 *
 * 1. This notice is required to be provided under our contract with the
 * U.S. Department of Energy (DOE). This work was produced at Lawrence Livermore
 * National Laboratory under Contract No. DE-AC52-07NA27344 with the DOE.
 *
 * 2. Neither the United States Government nor Lawrence Livermore National
 * Security, LLC nor any of their employees, makes any warranty, express or
 * implied, or assumes any liability or responsibility for the accuracy,
 * completeness, or usefulness of any information, apparatus, product, or
 * process disclosed, or represents that its use would not infringe
 * privately-owned rights.
 *
 * 3. Also, reference herein to any specific commercial products, process, or
 * services by trade name, trademark, manufacturer or otherwise does not
 * necessarily constitute or imply its endorsement, recommendation, or favoring
 * by the United States Government or Lawrence Livermore National Security,
 * LLC. The views and opinions of authors expressed herein do not necessarily
 * state or reflect those of the United States Government or Lawrence Livermore
 * National Security, LLC, and shall not be used for advertising or product
 * endorsement purposes.
 *
 */

#include <random>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef UFFD
extern "C" {
#include "../uffd_handler/uffd_handler.h"

volatile int stop_uffd_handler;
}
#endif

double get_wtime();

void create_files (const char* base_fname, int fnum, uint64_t file_size, bool do_fallocate);
void init_data    (const char* base_fname, int fnum, uint64_t file_size);
void sort_data    (const char* base_fname, int fnum, uint64_t file_size);
void validate_data(const char* base_fname, int fnum, uint64_t file_size);


int main(int argc, char** argv) {

  // for uffd
  int uffd;
  pthread_t uffd_thread;

  if(argc != 4) {
    std::cerr << "Usage: " << argv[0] << " <base_filename> <total GBytes> <num workers>" <<std::endl;
    return -1;
  }

  const char* base_fname = argv[1];
  float gbytes = atof(argv[2]);
  int num_workers = atoi(argv[3]);
  uint64_t file_size = gbytes*1024*1024*1024 / num_workers;
  bool do_fallocate = true;

  //
  // Create Files
  //
  double start_create = get_wtime();
  #pragma omp parallel for
  for(int i=0; i<num_workers; ++i) {
      create_files(base_fname, i, file_size, do_fallocate);
  }
  
  double stop_create = get_wtime();
  std::cout << "Files created: " << stop_create - start_create << " seconds." << std::endl;


  //
  // Init Data
  //
  double start_init = get_wtime();
  #pragma omp parallel for
  for(int i=0; i<num_workers; ++i) {
      init_data(base_fname, i, file_size);
  }

  double stop_init = get_wtime();
  std::cout << "Data Initialized: " << stop_init - start_init << " seconds." << std::endl;

  //
  // Sort Data
  //
  double start_sort = get_wtime();
#pragma omp parallel for
  for(int i=0; i<num_workers; ++i) {
      sort_data(base_fname, i, file_size);
  }

  double stop_sort = get_wtime();
  std::cout << "Files Sorted: " << stop_sort - start_sort << " seconds." << std::endl;


  //
  // Validate Data
  //
  double start_validate = get_wtime();
#pragma omp parallel for
  for(int i=0; i<num_workers; ++i) {
      validate_data(base_fname, i, file_size);
  }

  double stop_validate = get_wtime();
  std::cout << "Files Validated: " << stop_validate - start_validate << " seconds." << std::endl;

  return 0;
}

double get_wtime() {
  struct timeval now;
  gettimeofday(&now, NULL);
  return double(now.tv_sec) + double(now.tv_usec)/1e6;
}

uint64_t* mmap_open(const char* base_fname, int fnum, uint64_t file_size) {
  std::stringstream ssfilename;
  ssfilename << base_fname << "." << fnum;
  const char* filename = ssfilename.str().c_str();


  int open_options = O_RDWR;

  #ifdef O_LARGEFILE
    open_options != O_LARGEFILE;
  #endif

  int fd = open(ssfilename.str().c_str(), open_options, S_IRUSR|S_IWUSR);
  if(fd == -1) {
    std::stringstream error_msg;
    error_msg << "Error opening file " << filename;
    perror(error_msg.str().c_str());
    exit(-1);
  }

  void* mmap_addr = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(mmap_addr == MAP_FAILED) {
    std::stringstream error_msg;
    error_msg << "Error mmaping file " << filename;
    perror(error_msg.str().c_str());
    exit(-1);
  }


  if(close(fd) != 0) {
    std::stringstream error_msg;
    error_msg << "Error closing file (while still mmaped)" << filename;
    perror(error_msg.str().c_str());
    exit(-1);
  }

  return (uint64_t*) mmap_addr;
}

void mmap_close(void* addr, uint64_t file_size) {
   if(munmap(addr, file_size) != 0) {
     perror("Error closing MMAP");
     exit(-1);
   }
}

void create_files (const char* base_fname, int fnum, uint64_t file_size, bool do_fallocate) {
  std::stringstream ssfilename;
  ssfilename << base_fname << "." << fnum;
  const char* filename = ssfilename.str().c_str();

  if( access( filename, W_OK ) != -1 ) {
    remove(filename);
   }
  int open_options = O_RDWR | O_CREAT;
  #ifdef O_LARGEFILE
    open_options != O_LARGEFILE;
  #endif

  int fd = open(ssfilename.str().c_str(), open_options, S_IRUSR|S_IWUSR);
  if(fd == -1) {
    std::stringstream error_msg;
    error_msg << "Error opening file " << filename;
    perror(error_msg.str().c_str());
    exit(-1);
  }

  #ifdef __APPLE__
    if(ftruncate(fd, file_size) != 0) {
      std::stringstream error_msg;
      error_msg << "Error truncating file " << filename;
      perror(error_msg.str().c_str());
      exit(-1);
    }
  #else
    if(posix_fallocate(fd,0,file_size) != 0) {
      perror("Fallocate failed");
    }
  #endif

  if(close(fd) != 0) {
    std::stringstream error_msg;
    error_msg << "Error closing file (while still mmaped)" << filename;
    perror(error_msg.str().c_str());
    exit(-1);
  }
}

void init_data    (const char* base_fname, int fnum, uint64_t file_size) {
  uint64_t* pint = mmap_open(base_fname, fnum, file_size);
  madvise(pint, file_size, MADV_SEQUENTIAL);
  std::random_device rd;                                                                                                                                                                                                                                                                                                                                                                                                            
  std::mt19937 gen(rd());                                                                                                                                                                                                                                                                                                                                                                                                            
  std::uniform_int_distribution<uint64_t> rnd_int;
  for(int i=0; i<file_size / sizeof(uint64_t); ++i) {
    pint[i] = rnd_int(gen);;
  }
  mmap_close(pint, file_size);
}

void sort_data    (const char* base_fname, int fnum, uint64_t file_size) {
  uint64_t* pint = mmap_open(base_fname, fnum, file_size);
  madvise(pint, file_size, MADV_RANDOM);
  std::sort(pint, pint + (file_size / sizeof(uint64_t)));
  mmap_close(pint, file_size);
}
void validate_data(const char* base_fname, int fnum, uint64_t file_size) {
  uint64_t* pint = mmap_open(base_fname, fnum, file_size);
  madvise(pint, file_size, MADV_SEQUENTIAL);
  for(uint64_t i=1; i<file_size / sizeof(uint64_t); ++i) {
    if(pint[i] < pint[i-1]) {
      std::cerr << "Worker " << fnum << "found an error!" << std::endl;
      exit(-1);
    }
  }
  mmap_close(pint, file_size);
}


