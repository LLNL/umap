#include "umap/umap.h"
#include <string>

int main(){
  std::string filename(UMAP_SERVER_PATH);
  umap_server(filename);
  return 0;
}
