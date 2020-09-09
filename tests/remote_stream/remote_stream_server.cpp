//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

/*
 * An example of multiple remote memory objects over network
 */
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <omp.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <chrono>
#include <random>
#include "mpi.h"
#include "errno.h"
#include "umap/umap.h"
#include "umap/store/StoreNetwork.h"

#define ELEMENT_TYPE uint64_t

using namespace std;
using namespace std::chrono;

void reset_index(size_t *arr, size_t len, size_t range);

int main(int argc, char **argv)
{
  if( argc != 2 ){
    printf("Usage: %s [remote_array_bytes]\n",argv[0]);
    return 0;
  }
  
  const uint64_t array_length =  atoll(argv[1]);  
  
  /* bootstraping to determine server and clients usnig MPI */
  int rank, num_proc;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
  char hostname[256];
  if( gethostname(hostname, sizeof(hostname)) ==0 ) {
      cout << "Server "<< rank << "on "<<hostname
	   << " remote STREAM benchmark:: array_length = " << array_length << " bytes \n";
  }
  
  /* Prepare memory resources on the server */
  size_t umap_page_size = umapcfg_get_umap_page_size();
  size_t total_aligned_pages  = (array_length - 1)/umap_page_size + 1;
  size_t pages_per_server = total_aligned_pages/num_proc;
  if(rank==(num_proc-1))
    pages_per_server = total_aligned_pages - pages_per_server*rank;
  
  size_t aligned_size = umap_page_size * pages_per_server;  
  void* arr_a = malloc(aligned_size);
  void* arr_b = malloc(aligned_size);
  if( !arr_a || !arr_b ){
    std::cerr<<" Unable to allocate arrays on the server";
    return 0;
  }
  
  /* initialization function should be user defined */
  uint64_t *arr0 = (uint64_t*) arr_a;
  uint64_t *arr1 = (uint64_t*) arr_b;
  size_t num = aligned_size/sizeof(uint64_t);
  assert( aligned_size%sizeof(uint64_t) == 0);
  size_t offset = rank*(total_aligned_pages/num_proc*umap_page_size/sizeof(uint64_t));  
#pragma omp parallel for
  for(size_t i=0;i<num;i++){
    arr0[i]=offset+i;
    arr1[i]=offset+i;
  }

  /* Create two network-based datastores */
  int num_clients = 0;
  Umap::Store* ds0  = new Umap::StoreNetworkServer("arr_a", arr0, aligned_size);
  std::cout << "arr_a is Registed " << std::endl;
  
  Umap::Store* ds1  = new Umap::StoreNetworkServer("arr_b", arr1, aligned_size);
  std::cout << "arr_b is Registed " << std::endl;

  while(1)
    sleep(20);

  
  /* Free the network dastore */
  /* wait utill all clients are done */
  delete ds0;
  delete ds1;
  
  free(arr_a);
  free(arr_b);

  MPI_Finalize();
  
  return 0;
}
