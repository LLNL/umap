#include "UmapServiceManager.hpp"
#include "umap/util/Macros.hpp"
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

#ifndef SYS_memfd_create
#define SYS_memfd_create 319
#endif

int memfd_create(const char *name, unsigned int flags) {
  return syscall(SYS_memfd_create, name, flags);
}

long init_client_uffd() {
  struct uffdio_api uffdio_api;
  long uffd;

  /* Create and enable userfaultfd object */
  uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
  if (uffd == -1)
    perror("userfaultfd");

  uffdio_api.api = UFFD_API;
  uffdio_api.features = 0;
  if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
    perror("ioctl-UFFDIO_API");

  return uffd;
}


namespace Umap{
static uint64_t next_region_start_addr = 0x600000000000;
Umap::ClientManager* ClientManager::instance = NULL;
Umap::UmapServerManager* Umap::UmapServerManager::Instance=NULL;

int UmapServInfo::setup_remote_umap_handle(){
  int status = 0;
  ActionParam params;
  params.act = uffd_actions::umap;
  strcpy(params.name, filename.c_str());
  params.args = args;

  ::write(umap_server_fd, &params, sizeof(params));
  // recieve memfd and region size
  sock_fd_read(umap_server_fd, &(loc), sizeof(region_loc), &(memfd));
  std::cout<<"c: recv memfd ="<<memfd<<" sz ="<<std::hex<<loc.size<<std::endl;

  loc.base_addr = mmap(0, loc.size, PROT_READ|PROT_WRITE, MAP_SHARED, memfd, 0);
  if ((int64_t)loc.base_addr == -1) {
    perror("setup_uffd: map failed");
    exit(1);
  }

  std::cout<<"mmap:"<<std::hex<< loc.base_addr<<std::endl;
  //Tell server that the mmap is complete
  ::write(umap_server_fd, (void *)&loc.base_addr, sizeof(loc.base_addr));

  //Wait for the server to register the region to uffd
  sock_recv(umap_server_fd, (char*)&status, 1);
  std::cout<<"Done registering file to uffd"<<std::endl;
  return 0;
}

void UmapServInfo::remove_remote_umap_handle()
{
  int status = 0;
  ActionParam params;
  params.act = uffd_actions::unmap;
  strcpy(params.name, filename.c_str());
  ::write(umap_server_fd, &params, sizeof(params));
  sock_recv(umap_server_fd, (char*)&status, 1);
  std::cout<<"Done removing the uffd handling"<<std::endl;
  munmap(loc.base_addr, loc.size);
}

void ClientManager::setupUmapConnection(){
  int uffd;
  int dummy;
  if(!umap_server_fd){
    if(setup_uds_connection(&umap_server_fd, umap_server_path.c_str()) < 0){
      UMAP_LOG(Error, "unable to setup connection with file server");
      return;
    }
    uffd = init_client_uffd();
    sock_fd_write(umap_server_fd, &dummy, sizeof(int), uffd);
    ::close(uffd);
  }
}

void ClientManager::closeUmapConnection(){
  //Ideally this shouldn't require locks to be checked
  std::lock_guard<std::mutex> lock(cm_mutex);
  ::close(umap_server_fd);
}

void *submit_umap_req(const char *filename, int prot, int flags){
  ClientManager *cm = ClientManager::getInstance();
  return cm->map_req(std::string(filename), prot, flags);   
}

int submit_uunmap_req(const char *filename){
  ClientManager *cm = ClientManager::getInstance();
  cm->unmap_req(std::string(filename));
  return 0;
}

void terminateUmapClient(){
  ClientManager *cm = ClientManager::getInstance();
  cm->closeUmapConnection();
  ClientManager::deleteInstance();
}

UmapServInfo* ClientManager::cs_umap(std::string filename, int prot, int flags){
  umap_file_params args = {.prot = prot, .flags = flags};
  int dummy=0;
  int uffd=0;
  
  UmapServInfo *ret = NULL;
  if(file_conn_map.find(filename)!=file_conn_map.end()){ 
    //Todo:For multiple requests from multiple threads, it will need to be serialized
    UMAP_LOG(Error, "file already mapped for the application");
  }else{
    ret = new UmapServInfo(umap_server_fd, filename, args);
    file_conn_map[filename] = ret;
  }
  return ret;
}
      
void ClientManager::cs_uunmap(std::string filename){
  auto it = file_conn_map.find(filename);
  if(it == file_conn_map.end()){
    UMAP_LOG(Error,"No file mapped with the filename");
  }else{
    UmapServInfo* elem = it->second;
    file_conn_map.erase(it);
    delete elem;
  }
}

void* ClientManager::map_req(std::string filename, int prot, int flags){
  std::lock_guard<std::mutex> guard(cm_mutex);
  auto info = cs_umap(filename, prot, flags);
  if(info){
    return info->loc.base_addr;
  }else
    return NULL;
}

int ClientManager::unmap_req(std::string filename){
  std::lock_guard<std::mutex> guard(cm_mutex);
  auto it = file_conn_map.find(filename);
  if(it==file_conn_map.end()){
    UMAP_LOG(Debug, "unable to find connection with file server");
    return -1;
  }else{
    //TODO: Has to submit the unmap request to the server
    cs_uunmap(filename);
  }
}

int UmapServiceThread::start_thread(){
  if (pthread_create(&t, NULL, ThreadEntryFunc, this) != 0){
    UMAP_ERROR("Failed to launch thread");
    return -1;
  }
  else
    return 0;
}

void *UmapServiceThread::submitUmapRequest(std::string filename, int prot, int flags){
  struct stat st;
  int memfd=-1;
  int ffd = -1;
  char status;
  void *base_addr_local;
  void *base_addr_remote;
  unsigned long aligned_size;
  unsigned long page_size = 4096;

  std::lock_guard<std::mutex> task_lock(mgr->sm_mutex);
  mappedRegionInfo *map_reg = mgr->find_mapped_region(filename);
  if(!map_reg){
    ffd = open(filename.c_str(),O_RDONLY);
    if(ffd < 0){
      std::ostringstream errStream;
      errStream << "Error"<<__func__<<"("<<__FILE__<<":"<<__LINE__<<")"<<": Could not open file"<<filename;
      perror(errStream.str().c_str());
      //Existance of the file should be checked by the client
      exit(-1);
    }

    fstat(ffd, &st);
    memfd = memfd_create("uffd", 0);
    aligned_size = (st.st_size & ~(page_size - 1)) + page_size;
    ftruncate(memfd, aligned_size);
    mapped_files.push_back(filename);
    base_addr_local = mmap(0, aligned_size, PROT_READ|PROT_WRITE, MAP_SHARED,memfd, 0);
    map_reg = new mappedRegionInfo(ffd, memfd, base_addr_local, aligned_size);
    mgr->add_mapped_region(filename, map_reg);
    UMAP_LOG(Info,"filename:"<<filename<<" size "<<st.st_size<<" mmap local: 0x"<< std::hex << base_addr_local <<std::endl);
          //Todo: add error handling code
    //next_region_start_addr += aligned_size;
  }
  //Sending the memfd
  sock_fd_write(csfd, (char*)&(map_reg->reg), sizeof(region_loc), map_reg->memfd);
  //Wait for the memfd to get mapped by the client
  sock_recv(csfd, (char*)&base_addr_remote, sizeof(base_addr_remote));
  //uffd is already present with the UmapServiceThread
  std::cout<<"s: addr: "<<map_reg->reg.base_addr<<" uffd: "<<uffd<<" map_len="<<map_reg->reg.size<<std::endl;
  return Umap::umap_ex(map_reg->reg.base_addr, map_reg->reg.size, PROT_READ|PROT_WRITE, flags, map_reg->filefd, 0, NULL, true, uffd, base_addr_remote); //prot and flags need to be set 
}

int UmapServiceThread::submitUnmapRequest(std::string filename, bool client_term){
  std::lock_guard<std::mutex> task_lock(mgr->sm_mutex); 
  mappedRegionInfo *map_reg = mgr->find_mapped_region(filename);
  if(map_reg){
    if(Umap::uunmap_server(map_reg->reg.base_addr, map_reg->reg.size, uffd, map_reg->filefd, client_term)){ 
    //We could move the ref count of regions at this level
      munmap(map_reg->reg.base_addr, map_reg->reg.size);
      mgr->remove_mapped_region(filename);
    }
    return 0;
  }else{
    UMAP_LOG(Error, "No such file mapped");
    return -1;
  }
}

int UmapServiceThread::unmapClientFile(std::string filename){
  //Need reference counting here
  auto it = std::find(mapped_files.begin(), mapped_files.end(), filename);
  if(it!=mapped_files.end())
    mapped_files.erase(it);
  submitUnmapRequest(filename, false);
  return 0;
}

int UmapServiceThread::unmapClientFiles(){
  while(!mapped_files.empty()){
    std::string dfile = mapped_files.back();
    mapped_files.pop_back();
    submitUnmapRequest(dfile, true);
  }
  Umap::terminate_handler(uffd);
  return 0;
}

void* UmapServiceThread::serverLoop(){
  ActionParam params;
  int nready;
  struct pollfd pollfds[2]={{ .fd = csfd, .events = POLLIN, .revents = 0 },
           { .fd = pipefds[0], .events = POLLIN | POLLRDHUP | POLLPRI, .revents = 0 }};
  for(;;){
    //Do poll to determine if the client has died
    nready = poll(pollfds, 2, -1);
    if(nready==-1 || pollfds[1].revents){
      break;
    }
    //get the filename and the action from the client
    if(::read(csfd, &params, sizeof(params)) == 0)
      break;
    //decode if it is a request to unmap or map
    if(params.act == uffd_actions::umap){
      std::string filename(params.name);
      submitUmapRequest(filename, params.args.prot, params.args.flags);
    }else{
      std::string filename(params.name);
      unmapClientFile(filename);
      //yet to implement submitUnmapRequest
    }
    //operation completed
    ::write(csfd, "\x00", 1);
    pollfds[0].revents = 0;
    pollfds[1].revents = 0;
  }
  unmapClientFiles();
  mgr->removeServiceThread(csfd);
}

void UmapServerManager::removeServiceThread(int csfd){
  //Need to check if we need a unique_lock
  std::lock_guard<std::mutex> task_lock(sm_mutex); 
  auto it = service_threads.find(csfd);
  if(it == service_threads.end()){
    UMAP_LOG(Error,"No threads found for given connection");
  }else{
    UmapServiceThread *t = it->second;
    service_threads.erase(it);
    delete(t);
  }
}

void UmapServerManager::start_service_thread(int csfd, int uffd){
  std::lock_guard<std::mutex> task_lock(sm_mutex); 
  UmapServiceThread *t = new UmapServiceThread(csfd, uffd, this);
  if(t && !t->start_thread())
    service_threads[csfd] = t;
}

void UmapServerManager::stop_service_threads(){
  std::lock_guard<std::mutex> task_lock(sm_mutex); 
  auto it=service_threads.begin();
  while(it!=service_threads.end()){
    UmapServiceThread *t = it->second;
    t->stop_thread();
  }
}

void UmapServerManager::add_mapped_region(std::string filename, mappedRegionInfo* m){
  file_to_region_map[filename] = m;
}

void start_umap_service(int csfd){
  int dummy;
  int uffd;
  UmapServerManager *usm = UmapServerManager::getInstance();
  sock_fd_read(csfd, &dummy, sizeof(int), &uffd);
  usm->start_service_thread(csfd, uffd);
}

void connectUmap(std::string sock_path){
  ClientManager *cm = ClientManager::getInstance(sock_path);
  cm->setupUmapConnection();
}

} //End of Umap namespace

void init_umap_client(std::string sock_path){
  Umap::connectUmap(sock_path);
}

void* client_umap(const char *filename, int prot, int flags){
  return Umap::submit_umap_req(filename, prot, flags);
}

int client_uunmap(const char *filename){
  return Umap::submit_uunmap_req(filename);
}

void close_umap_client(){
  Umap::terminateUmapClient();
}

void umap_server(std::string sock_path){
  int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un addr;

  memset(&addr, 0, sizeof(addr));
  snprintf(addr.sun_path, sizeof(addr.sun_path), sock_path.c_str());
  addr.sun_family = AF_UNIX;
  unlink(addr.sun_path);
  bind(sfd, (struct sockaddr*)&addr, sizeof(addr));
        
  listen(sfd, 256);
  for (;;) {
    int cs = accept(sfd, 0, 0);
    if (cs == -1) {
      perror("accept");
      exit(1);
    }
    Umap::start_umap_service(cs);
  }
  close(sfd);
  unlink(addr.sun_path);
}
