#include "UmapServiceManager.hpp"
#include "umap/util/Macros.hpp"

static uint64_t next_region_start_addr = 0x600000000000;

namespace Umap{
Umap::UmapServerManager* Umap::UmapServerManager::Instance=NULL;

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
  void *base_mmap_local;
  void *base_addr_remote;
  unsigned long mmap_size;
  unsigned long umap_page_size = umapcfg_get_umap_page_size();

  std::lock_guard<std::mutex> task_lock(mgr->sm_mutex);
  mappedRegionInfo *map_reg = mgr->find_mapped_region(filename);
  if(!map_reg){
    ffd = open(filename.c_str(),O_LARGEFILE | O_DIRECT| O_RDONLY);
    if(ffd < 0){
      std::ostringstream errStream;
      errStream << "Error"<<__func__<<"("<<__FILE__<<":"<<__LINE__<<")"<<": Could not open file"<<filename;
      perror(errStream.str().c_str());
      //Existance of the file should be checked by the client
      exit(-1);
    }

    fstat(ffd, &st);
    memfd = memfd_create("uffd", 0);
    mmap_size = get_mmap_size(st.st_size, umap_page_size);
    ftruncate(memfd, mmap_size);
    mapped_files.push_back(filename);
    base_mmap_local = mmap((void *)0, mmap_size, PROT_READ|PROT_WRITE, MAP_SHARED,memfd, 0);
    map_reg = new mappedRegionInfo(ffd, memfd, base_mmap_local, get_mmap_size(st.st_size, umap_page_size));
    mgr->add_mapped_region(filename, map_reg);
    UMAP_LOG(Debug,"filename:"<<filename<<" size "<<st.st_size<<" mmap local: 0x"<< std::hex << base_mmap_local <<std::dec<<std::endl);
          //Todo: add error handling code
    //next_region_start_add += alignee_size;
  }
  //Sending the memfd
  sock_fd_write(csfd, (char*)&(map_reg->reg), sizeof(region_loc), map_reg->memfd);
  //Wait for the memfd to get mapped by the client
  sock_recv(csfd, (char*)&base_addr_remote, sizeof(base_addr_remote));
  //uffd is already present with the UmapServiceThread
  std::cout<<"s: addr: "<<map_reg->reg.base_addr<<" uffd: "<<uffd<<" map_len="<<get_mmap_size(map_reg->reg.size, umap_page_size)<<std::endl;
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
  struct umap_cfg_data init_pkt;
 
  init_pkt.umap_page_size = umapcfg_get_umap_page_size();
  init_pkt.max_fault_events = umapcfg_get_max_fault_events();
  init_pkt.num_fillers = umapcfg_get_num_fillers();
  init_pkt.num_evictors = umapcfg_get_num_evictors();
  init_pkt.max_pages_in_buffer = umapcfg_get_max_pages_in_buffer();
  init_pkt.low_water_threshold = umapcfg_get_evict_low_water_threshold();
  init_pkt.high_water_threshold = umapcfg_get_evict_high_water_threshold();

  UmapServerManager *usm = UmapServerManager::getInstance();
  std::cout<<"Receiving dummy"<<std::endl;
  sock_fd_read(csfd, &dummy, sizeof(int), &uffd);
  std::cout<<"Dummy received"<<std::endl;
  ::write(csfd, &init_pkt, sizeof(struct umap_cfg_data));
  std::cout<<"Sending init packet with num_fillers = "<<init_pkt.num_fillers<<std::endl;
  ::read(csfd, &dummy, sizeof(dummy));
  std::cout<<"Received ack!"<<std::endl;
  usm->start_service_thread(csfd, uffd);
}

} //End of Umap namespace

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
