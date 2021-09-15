#include "umap/umap.h"
#include <string>
#include <iostream>

int main(int argc, char **argv){
  std::string filename;
  if(argc < 2){
        filename = std::string(UMAP_SERVER_PATH);
        std::cout<<"Using default mp-umapd socket path";
	std::cout<<"Usage: umap-server <unix socket path>";
  }else{
  	filename = std::string(argv[1]);
  }
  umap_server(filename.c_str());
  return 0;
}
