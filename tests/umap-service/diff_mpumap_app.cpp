/*
 We rename the header file to avoid name conflict
 with umap header. This allows mp-umap and umap to
 co-exist while enabling future integration with umap 
 easier. Hence the commented test code needs to be 
 used when compiled outside the source tree

#include "umap/mpumapclient.h"
#include <unistd.h>
#include <sys/mman.h>
using namespace std;
#include <iostream>

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
  void *mapped_addr, *mapped_addr2;;
  char *read_addr, *read_addr2;
  bool diff = false;
  int i=0;
  std::string sock_path, filename1, filename2;
  int opt;
  char opt_string[] = "c:s:f:";
  while((opt = getopt(argc, argv, opt_string)) != -1){
    switch (opt) {
      case 'c': 
        sock_path = std::string(optarg);
        break;
      case 'f':
        filename1 = std::string(optarg);
        break;
      case 's':
        filename2 = std::string(optarg);
        break;
    }
  }
  if(filename1.empty() || filename2.empty()){
    std::cerr<<"Usage: ./umap-client -c <socket_path> -f <file1_name> -s <file2_name>";
    exit(-1);
  }
 
  if(sock_path.empty()){
    sock_path = std::string(UMAP_SERVER_PATH);
  } 
  init_umap_client(sock_path.c_str());
  disp_umap_env_variables(); 
  mapped_addr = client_umap(filename1.c_str(), PROT_READ, MAP_SHARED|MAP_FIXED, NULL);
  mapped_addr2 = client_umap(filename2.c_str(), PROT_READ, MAP_SHARED|MAP_FIXED, NULL);
  for(read_addr = (char *)mapped_addr, read_addr2 = (char *)mapped_addr2; (read_addr - mapped_addr) < 4096*1024 ; read_addr += 4096, read_addr2 += 4096 ){
    if(*read_addr != *read_addr2){
        diff = true;
        std::cout<<"Files differ at offset"<<(unsigned long)(read_addr - mapped_addr)<<std::endl; 
        break;
    }
  }
  if(!diff){
    std::cout<<"No difference detected at page boundaries"<<std::endl;
  }
  client_uunmap(filename1.c_str()); 
  client_uunmap(filename2.c_str()); 
  return 0;
}
*/

#include "umap/umap-client.h"
#include <unistd.h>
#include <sys/mman.h>
using namespace std;
#include <iostream>

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
  void *mapped_addr, *mapped_addr2;;
  char *read_addr, *read_addr2;
  bool diff = false;
  int i=0;
  std::string sock_path, filename1, filename2;
  int opt;
  char opt_string[] = "c:s:f:";
  while((opt = getopt(argc, argv, opt_string)) != -1){
    switch (opt) {
      case 'c': 
        sock_path = std::string(optarg);
        break;
      case 'f':
        filename1 = std::string(optarg);
        break;
      case 's':
        filename2 = std::string(optarg);
        break;
    }
  }
  if(filename1.empty() || filename2.empty()){
    std::cerr<<"Usage: ./umap-client -c <socket_path> -f <file1_name> -s <file2_name>";
    exit(-1);
  }
 
  if(sock_path.empty()){
    sock_path = std::string(UMAP_SERVER_PATH);
  } 
  init_umap_client(sock_path.c_str());
  disp_umap_env_variables(); 
  mapped_addr = client_umap(filename1.c_str(), PROT_READ, MAP_SHARED, NULL);
  mapped_addr2 = client_umap(filename2.c_str(), PROT_READ, MAP_SHARED, NULL);
  char *end_addr = (char *)mapped_addr + 4096*1024;
  for(read_addr = (char *)mapped_addr, read_addr2 = (char *)mapped_addr2; read_addr <= end_addr ; read_addr++, read_addr2++ ){
    if(*read_addr != *read_addr2){
        diff = true;
        std::cout<<"Files differ at offset "<<(unsigned long)read_addr - (unsigned long)mapped_addr<<std::endl; 
        break;
    }
  }
  if(!diff){
    std::cout<<"No difference detected at page boundaries"<<std::endl;
  }
  client_uunmap(filename1.c_str()); 
  client_uunmap(filename2.c_str()); 
  return 0;
}
