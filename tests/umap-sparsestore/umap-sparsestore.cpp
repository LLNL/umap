#include <iostream>
#include <umap/umap.h>
#include <umap/store/SparseStore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

using namespace std;

void populate_region(uint64_t* region, uint64_t region_length){
#pragma omp parallel for
  for(uint64_t i=0; i < region_length; ++i)
    region[i] = (uint64_t) (region_length - i);
}

int remove_directory(std::string root_path){
  DIR *directory;
  struct dirent *ent;
  if ((directory = opendir(root_path.c_str())) != NULL){
    while ((ent = readdir(directory)) != NULL){
      std::string filename_str(ent->d_name);
      std::string file_path = root_path + "/" + filename_str; 
      unlink(file_path.c_str());
    }
    closedir(directory);
    if( remove(root_path.c_str()) != 0 ){
      perror("Error deleting directory from new function");
      return -1;
    }
    else{
      std::cout << "Successfully deleted directory" << std::endl;
      return 0;
    }
  } else {
    perror("Error deleting directory");
    return -1; 
  }
}

int main(int argc, char *argv[]){
  void *region;
  unsigned long page_size = 16384;
  uint64_t region_size_bytes = 1073741824;
  size_t sparsestore_file_granularity = 134217728;
  unsigned long long map_len;
  struct stat st;
  int fd;
  void* start_addr = (void *)0x600000000000;// NULL;
  unsigned long i=0;
  if(argc < 2){
    std::cout<<"Please provide a base directory name";
    exit(-1);
  }
  
  std::string root_path = argv[1];
  struct stat info;

  if(stat( root_path.c_str(), &info ) == 0){
     remove_directory(root_path.c_str());
  }
  { //start "create" scope
  Umap::SparseStore* store;
  
  store = new Umap::SparseStore(region_size_bytes,page_size,root_path,sparsestore_file_granularity);

  
  // call umap
  int flags = UMAP_PRIVATE;
     
 //  if (start_addr != nullptr)
  flags |= MAP_FIXED;

  const int prot = PROT_READ|PROT_WRITE;
  region = umap_ex(start_addr, region_size_bytes, prot, flags, -1, 0, store);
  
  if ( region == UMAP_FAILED ) {
    perror("UMap failed...");
    delete store;
    exit(-1);
  }

  // Start: populate region as array of ints
  uint64_t *arr = (uint64_t *) region;
  uint64_t arraysize = region_size_bytes/sizeof(uint64_t);

  populate_region(arr,arraysize);
  // End:  populate region as array of ints

  // unmap
  if (uunmap(region, region_size_bytes) < 0) {
       perror("uunmap failed..");
       exit(-1);
  }
  int sparse_store_close_files = store->close_files();
  if (sparse_store_close_files != 0 ){
    std::cerr << "Error closing SparseStore files" << std::endl;
    delete store;
    exit(-1);
  }
  delete store;

  }
  {
  // open
  Umap::SparseStore* store;
  store = new Umap::SparseStore(root_path,false);

  

  int flags = UMAP_PRIVATE;
     
  // if (start_addr != nullptr)
  flags |= MAP_FIXED;

  const int prot = PROT_READ;//|PROT_WRITE;


  // call umap
  region = umap_ex(start_addr, region_size_bytes, prot, flags, -1, 0, store);
  
  if ( region == UMAP_FAILED ) {
    perror("UMap failed...");
    delete store;
    exit(-1);
  }

  // Start: unmap
  if (uunmap(region, region_size_bytes) < 0) {
       perror("uunmap failed..");
       exit(-1);
  }
  int sparse_store_close_files = store->close_files();
  if (sparse_store_close_files != 0 ){
    std::cerr << "Error closing SparseStore files" << std::endl;
    delete store;
    exit(-1);
  }
  delete store;
  // End: unmap

  // End:   repeat
  }
  return 0;
}
