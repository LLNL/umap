#include <iostream>
#include "umap/umap-client.h"
#include <sys/mman.h>
using namespace std;

void disp_umap_env_variables() {
  std::cout
      << "Environment Variable Configuration (command line arguments obsolete):\n"
      << "UMAP_PAGESIZE                   - currently: " << umapcfg_get_umap_page_size() << " bytes\n"
      << "UMAP_PAGE_FILLERS               - currently: " << umapcfg_get_num_fillers() << " fillers\n"
      << "UMAP_PAGE_EVICTORS              - currently: " << umapcfg_get_num_evictors() << " evictors\n"
      << "UMAP_BUFSIZE                    - currently: " << umapcfg_get_max_pages_in_buffer() << " pages\n"
      << "UMAP_EVICT_LOW_WATER_THRESHOLD  - currently: " << umapcfg_get_evict_low_water_threshold() << " percent full\n"
      << "UMAP_EVICT_HIGH_WATER_THRESHOLD - currently: " << umapcfg_get_evict_high_water_threshold()
      << " percent full\n"
      << std::endl;
}

int main(int argc, char *argv[]){
  void *mapped_addr, *mapped_addr2;
  char *read_addr;
  unsigned long page_size = 16384;
  unsigned long i=0;
  if(argc < 2){
    std::cout<<"Please provide a filename to umap";
    exit(-1);
  }

  unsigned long accumulate;
  init_umap_client(UMAP_SERVER_PATH);
  disp_umap_env_variables(); 
  mapped_addr = client_umap(argv[1], PROT_READ, MAP_SHARED, NULL);
//    mapped_addr2 = client_umap(argv[2], PROT_READ, MAP_SHARED);
  //int numthreads = omp_get_num_threads();  
  //printf("Number of threads = %d\n",numthreads);
  #pragma omp parallel for schedule(static,2) private(read_addr)
  for(i=0;i<1024;i++){
    read_addr  = (char *)mapped_addr + i * 16384;
    char val = *read_addr;
    val *= val;
//    accumulate += val;
//    cout<<"Jumped to next page"<<reinterpret_cast<void *>(read_addr)<<endl;
//    val = *read_addr2;
//    cout<<"Jumped to second file page"<<reinterpret_cast<void *>(read_addr2)<<endl;
  }
  client_uunmap(argv[1]); 
  
#if 0  
//  mapped_addr = client_umap(argv[1], PROT_READ, MAP_SHARED);
  for(i=0, read_addr=(char *)mapped_addr ;i<100 ;i++,read_addr+=16384){
    char val = *read_addr;
    cout<<"Jumped to next page"<<reinterpret_cast<void *>(read_addr)<<endl;
  }
  client_uunmap(argv[1]); 
#endif
  close_umap_client();
}
