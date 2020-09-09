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


int main(int argc, char **argv)
{
  if( argc != 3 ){
    printf("Usage: %s [per_array_bytes] [num_repeat]\n",argv[0]);
    return 0;
  }
  
  const uint64_t umap_pagesize = umapcfg_get_umap_page_size();
  const uint64_t array_length  = atoll(argv[1]);
  const int num_repeats = atoi(argv[2]);
  assert( array_length % umap_pagesize == 0);
  char hostname[256];
  assert( gethostname(hostname, sizeof(hostname)) ==0 );


  /* bootstraping to determine server and clients usnig MPI */
  int rank,num_proc;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
  if(rank==0){
    cout << "umap_pagesize "  << umap_pagesize << "\n";
    cout << "Remote STREAM Add :: array_length = "  << array_length << " bytes \n";    
  }

  /* Start registering network-based datastores */
  /* successful only if the server has published the object */
  auto timing_map_st = high_resolution_clock::now();

  /*  Umap::Store* ds0 = new Umap::StoreNetworkClient("arr_a", array_length);
  assert(ds0!=NULL);
  cout << "Rank "<< rank << " registered arr_a" << "\n";

  Umap::Store* ds1 = new Umap::StoreNetworkClient("arr_b", array_length);
  assert(ds1!=NULL);
  cout << "Rank "<< rank << " registered arr_b" << "\n";
  MPI_Barrier(MPI_COMM_WORLD);
    
  // map to the remote memory region
  void* region_addr = NULL;
  int   prot        = PROT_READ;
  int   flags       = UMAP_PRIVATE;
  int   fd          = -1;
  off_t offset      = 0;
  void* arr_a = umap_ex(region_addr,
			array_length,
			prot, flags,
			fd, offset,
			ds0);
  void* arr_b = umap_ex(region_addr,
			array_length,
			prot, flags,
			fd, offset,
			ds1);
*/
  
  void* arr_a = umap_network("arr_a", NULL, array_length);
  void* arr_b = umap_network("arr_b", NULL, array_length);

  auto timing_map_end = high_resolution_clock::now();

  
  if ( arr_a == UMAP_FAILED || arr_b == UMAP_FAILED) {
    std::cerr << "Failed to umap network-based datastore " << std::endl;
    return 0;
  }
  
  auto timing_map = duration_cast<microseconds>(timing_map_end - timing_map_st);
  cout << "Rank " << rank << " hostname " << hostname << "\n";
  cout << "Rank " << rank << " arr_a "<< arr_a << " arr_b "<< arr_b
       <<", Map Time [us]: "<< timing_map.count() <<"\n"<<std::flush;

  const size_t num_elements = array_length/sizeof(ELEMENT_TYPE);
  size_t num_elements_per_client = (num_elements-1)/num_proc + 1;
  size_t id_st = num_elements_per_client*rank;
  size_t id_end = id_st + num_elements_per_client;
  id_end = (id_end>num_elements) ?num_elements : id_end;
  cout << "Rank " << rank << " arr[ " << id_st <<", " << id_end << ") \n";


  ELEMENT_TYPE *a = (ELEMENT_TYPE *) arr_a;
  ELEMENT_TYPE *b = (ELEMENT_TYPE *) arr_b;
  ELEMENT_TYPE *c = (ELEMENT_TYPE *) malloc(array_length);
  assert( c!=NULL);
  
  /* Main loop: update num_updates times to the buffer for num_periods times */
  MPI_Barrier(MPI_COMM_WORLD);
  auto timing_update_st = high_resolution_clock::now();      
  for( int p=0; p<num_repeats; p++ ){
#pragma omp parallel for
    for(size_t i=id_st; i<id_end; i++){
      c[i] = a[i] + b[i];
    }
  }
  auto timing_update_end = high_resolution_clock::now();
  auto timing_update = duration_cast<microseconds>(timing_update_end - timing_update_st);
  /* End of Main Loop */

  size_t time = timing_update.count()/num_repeats;
  size_t bytes= array_length*3;

  if(c[(id_st+id_end)/2] != (id_st+id_end) ){
    cout << "Error: Client " << rank << " c["<< (id_st+id_end)/2 <<"]="<< c[(id_st+id_end)/2] <<" \n";
  }else if(rank==0){
    cout << "Rank " << rank
	 << " Bandwidth [MB/s] : " << bytes*1.0/time 
	 << " Ave. time [us] : "   << time << std::endl;
  }
  assert( c[(id_st+id_end)/2] == (id_st+id_end) ) ;
  MPI_Barrier(MPI_COMM_WORLD);
  
  /* Unmap file */
  if ( uunmap(arr_a, array_length)<0  || uunmap(arr_b, array_length)<0 ) {
    int eno = errno;
    std::cerr << "Failed to unmap network datastore: " << strerror(eno) << endl;
    return -1;
  }

  /* Free the network dastore */
  //delete ds0;
  //delete ds1;
  free(c);


  MPI_Finalize();
  
  return 0;
}
