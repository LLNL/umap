#include <iostream>
#include <algorithm>
#include <mutex>
#include <linux/userfaultfd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <sys/stat.h>
#include "socket.hpp"

#define NAME_SIZE 100
#define UMAP_SERVER_PATH "/tmp/umap-server"
#ifndef SYS_memfd_create
#define SYS_memfd_create 319
#endif

enum class uffd_actions: int{
  umap,
  unmap,
};
typedef struct{
  uint64_t size;
  void *base_addr;
  uint64_t page_size;
  uint64_t len_diff;
}region_loc;
 
typedef struct {
  int prot;
  int flags;
  void* fixed_base_addr;
}umap_file_params;

typedef struct{
  uffd_actions act;
  char name[NAME_SIZE];
  umap_file_params args;
}ActionParam;

struct umap_cfg_data{
    uint64_t umap_page_size;
    uint64_t max_fault_events;
    uint64_t num_fillers;
    uint64_t num_evictors; 
    uint64_t max_pages_in_buffer;
    int      low_water_threshold;
    int      high_water_threshold; 
};

int memfd_create(const char *name, unsigned int flags);
unsigned long get_aligned_size(unsigned long fsize, unsigned long page_size);
unsigned long get_mmap_size(unsigned long fsize, unsigned long page_size);
void *get_umap_aligned_base_addr(void *mmap_addr, uint64_t page_size);
