#include "umap/umap-client.h"
#include <unistd.h>
#include <sys/mman.h>
using namespace std;
#include <iostream>


int main(int argc, char *argv[]){
  void *mapped_addr, *mapped_addr2;;
  char *read_addr, *read_addr2;
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
  mapped_addr = client_umap(filename1.c_str(), PROT_READ, MAP_SHARED|MAP_FIXED, NULL);
  mapped_addr2 = client_umap(filename2.c_str(), PROT_READ, MAP_SHARED|MAP_FIXED, NULL);
  for(i = 0, read_addr = (char *)mapped_addr, read_addr2 = (char *)mapped_addr2; i < 1000 ; i++, read_addr += 4096, read_addr2 += 4096 ){
    char val = *read_addr;
    cout<<"Jumped to next page"<<val<<endl;
    val = *read_addr2;
    cout<<"Jumped to second file page"<<val<<endl;
  }
  std::cout<<"Files differ at offset"<<(unsigned long)(read_addr - mapped_addr)<<std::endl; 
  return 0;
}
