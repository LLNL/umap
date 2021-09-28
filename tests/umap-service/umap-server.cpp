//////////////////////////////////////////////////////////////////////////////
//// Copyright 2017-2021 Lawrence Livermore National Security, LLC and other
//// UMAP Project Developers. See the top-level LICENSE file for details.
////
//// SPDX-License-Identifier: LGPL-2.1-only
////////////////////////////////////////////////////////////////////////////////

/*
 We rename the header file to avoid name conflict
 with umap header. This allows mp-umap and umap to
 co-exist while enabling future integration with umap 
 easier. Hence the commented test code needs to be 
 used when compiled outside the source tree
 
#include "umap/mpumapd.h"
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
*/
 
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
