#include "umap/umap-client.h"
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
using namespace std;
#include <iostream>

long get_file_size(std::string filename)
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

int main(int argc, char *argv[]){
  void *mapped_addr, *mapped_addr2;;
  char *read_addr, *read_addr2;
  int total=0;
  std::string sock_path, filename1;
  int opt;
  char opt_string[] = "c:f:";
  while((opt = getopt(argc, argv, opt_string)) != -1){
    switch (opt) {
      case 'c': 
        sock_path = std::string(optarg);
        break;
      case 'f':
        filename1 = std::string(optarg);
        break;
    }
  }
  if(filename1.empty()){
    std::cerr<<"Usage: ./umap-client -c <socket_path> -f <file1_name>";
    exit(-1);
  }
 
  if(sock_path.empty()){
    sock_path = std::string(UMAP_SERVER_PATH);
  } 
  init_umap_client(sock_path.c_str());
  mapped_addr = client_umap(filename1.c_str(), PROT_READ, MAP_SHARED|MAP_FIXED, NULL);
  long flen = get_file_size(filename1);
  char *end_addr = (char *)mapped_addr + flen - 1;
  for(read_addr = (char *)mapped_addr; read_addr <= end_addr; read_addr+=4096){
    total += *read_addr; 
  }
  std::cout<<"total = %ld "<<total<<std::endl;
  return 0;
}
