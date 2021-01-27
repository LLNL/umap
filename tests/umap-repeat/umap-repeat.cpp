#include <iostream>
#include "umap/umap.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

using namespace std;

int main(int argc, char *argv[]){
  void *region;
  unsigned long page_size = 16384;
  unsigned long long map_len;
  struct stat st;
  int fd;
  
  unsigned long i=0;
  if(argc < 2){
    std::cout<<"Please provide a filename to umap";
    exit(-1);
  }
  fd = open(argv[1], O_RDWR | O_LARGEFILE | O_DIRECT, S_IRUSR | S_IWUSR);
  fstat(fd, &st);
  map_len = st.st_size;
  map_len = map_len & ~(page_size - 1);
  map_len += page_size;
  
  for (int i=0; i < 20 ;i++){

  	region = umap((void *)0x600000000000,map_len,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_FIXED,fd, 0);
        if(region)
        {
		*(int *)(region + page_size * i) = i;
        }
        else{
  		perror("umap failed read write");
		exit(-1);
        }
        uunmap(region, map_len);

        region = umap((void *)0x600000000000,map_len,PROT_READ,MAP_PRIVATE|MAP_FIXED,fd, 0);
        if(region){
        	int val = *(int *)(region + page_size * i);
                printf("Value read at location 0x%lx, val=%d\n",region + page_size * i,val);
#if 0 
		if(val!=i){
			printf("Error with read value");
		}
#endif
        }else{
		perror("umap failed read only");
		exit(-1);
	}
        uunmap(region, map_len);
  }
  close(fd);
}
