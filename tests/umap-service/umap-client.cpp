#include <iostream>
#include "umap/umap.h"
#include <sys/mman.h>
using namespace std;

int main(int argc, char *argv[]){
  void *mapped_addr, *mapped_addr2;;
  char *read_addr, *read_addr2;
  int i=0;
  if(argc < 2){
    std::cout<<"Please provide a filename to umap";
    exit(-1);
  }

  init_umap_client(std::string(UMAP_SERVER_PATH));
  for(int i=0;i<2;i++)
  {
    mapped_addr = client_umap(argv[1], PROT_READ, MAP_SHARED);
    mapped_addr2 = client_umap(argv[2], PROT_READ, MAP_SHARED);
    for(i=0, read_addr=(char *)mapped_addr, read_addr2=(char *)mapped_addr2;i<10000 ;i++,read_addr+16384, read_addr2+4096){
      char val = *read_addr;
      cout<<"Jumped to next page"<<val<<endl;
      val = *read_addr2;
      cout<<"Jumped to second file page"<<val<<endl;
    }
    client_uunmap(argv[1]); 
    client_uunmap(argv[2]); 
  }

  mapped_addr = client_umap(argv[1], PROT_READ, MAP_SHARED);
  for(i=0, read_addr=(char *)mapped_addr ;i<100 ;i++,read_addr+16384){
    char val = *read_addr;
    cout<<"Jumped to next page"<<val<<endl;
  }
  client_uunmap(argv[1]); 

  close_umap_client();
}
