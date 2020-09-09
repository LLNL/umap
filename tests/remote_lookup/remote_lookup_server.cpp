//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

/*
 * An example showing UMap remote memory regions over network
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
    printf("Usage: %s [total_memory_region_bytes] \n",argv[0]);
    return 0;
  }
  
  const uint64_t umap_region_length =  atoll(argv[1]);
  
  /* bootstraping to determine server and clients using MPI */
  int rank, num_proc;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &num_proc);

  
  /* Prepare memory resources on the server */
  size_t umap_page_size = umapcfg_get_umap_page_size();
  size_t total_aligned_pages  = (umap_region_length - 1)/umap_page_size + 1;
  size_t pages_per_server = total_aligned_pages/num_proc;
  size_t aligned_pages = pages_per_server;
  if(rank==(num_proc-1))
    aligned_pages = total_aligned_pages - pages_per_server*(num_proc-1);
  
  size_t aligned_size = umap_page_size * aligned_pages;  
  void* server_buffer = malloc(aligned_size);  
  if(!server_buffer){
    std::cerr<<" Unable to allocate " << umap_region_length << " bytes on the server";
    return 0;
  }else{
    char hostname[256];
    if( gethostname(hostname, sizeof(hostname)) == 0 ) 
      cout << "Server " << rank << " on hostname " << hostname << "\n";
  }
  
  /* initialization function should be user defined */
  uint64_t *arr = (uint64_t*) server_buffer;
  size_t num_elements = aligned_size/sizeof(uint64_t);
  size_t offset = pages_per_server*umap_page_size/sizeof(uint64_t)*rank;
#pragma omp parallel for
  for(size_t i=0;i<num_elements;i++)
    arr[i]=i+offset;

  /* Create a network-based datastore */
  /* 0 num_clients leaves the server on */
  Umap::Store* datastore  = new Umap::StoreNetworkServer("a",
							 server_buffer,
							 aligned_size);
  
  while(1)
    sleep(10);
  
  /* Free the network dastore */
  delete datastore;
    
  free(server_buffer);
  
  return 0;
}
