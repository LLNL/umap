#include <string>
#include <thread>
#include <vector>
#include <map>
#include <mutex>

#include "umap.h"
#include "socket.hpp"
#include <unistd.h>
#include <poll.h>
#include "umap/util/Macros.hpp"
#include "UmapServiceCommon.hpp"

namespace Umap{
  class UmapServerManager;
  class UmapServiceThread;

  class mappedRegionInfo{
    friend class UmapServiceThread;
    private:
      int memfd;
      region_loc reg;
      region_loc remote_reg;
      int filefd;
    public:
       mappedRegionInfo(int fd, int mfd, void *b, uint64_t l):filefd(fd), memfd(mfd){ reg.base_addr = b; reg.size = l; reg.page_size = umapcfg_get_umap_page_size();}
      ~mappedRegionInfo(){}
  };
   
  class UmapServiceThread{
    private:
      int                  csfd;
      pthread_t            t;
      UmapServerManager*   mgr;
      int                  uffd;
      int                  pipefds[2];
      std::vector<std::string>  mapped_files;
      int unmapClientFiles();
      int unmapClientFile(std::string filename);
      void *serverLoop();
      static void *ThreadEntryFunc(void *p){
        return ((UmapServiceThread*)p)->serverLoop();
      }
      //Lock: These are the two functions that update UmapServerManager datastructures, so they need to acquire lock
      void *submitUmapRequest(std::string filename, int prot, int flags);
      int submitUnmapRequest(std::string filename, bool client_term=false);
      //End of interfaces that lock
    public:
      ~UmapServiceThread(){ ::close(uffd); }
      UmapServiceThread(uint64_t fd, int ufd, UmapServerManager *m):csfd(fd),mgr(m),uffd(ufd){
         pipe(pipefds); 
      }
      int start_thread();
      void stop_thread(){
        ::write(pipefds[1],0,1); 
      }
  };
   
  class UmapServerManager{
      friend class UmapServiceThread;
    private:
      std::mutex sm_mutex;
      static UmapServerManager *Instance;
      std::map<std::string, mappedRegionInfo*> file_to_region_map;
      std::map<int, UmapServiceThread*> service_threads;
      //vector<UmapServiceThread*> zombie_list;     

      UmapServerManager(){}
      mappedRegionInfo *find_mapped_region(std::string filename){
        auto it = file_to_region_map.find(filename);
        if(it==file_to_region_map.end()){
          return NULL;
        }else{
          return it->second;
        }
      }
      void add_mapped_region(std::string filename, mappedRegionInfo* m);
      void remove_mapped_region(std::string filename){
        auto it = file_to_region_map.find(filename);
        if(it!=file_to_region_map.end()){
          delete it->second;
          file_to_region_map.erase(it);
        }
      };
    public:
      static UmapServerManager *getInstance(){
        if(!Instance)
          Instance = new UmapServerManager();
        return Instance;
      }
      //Start of Interfaces that obtain lock
      void start_service_thread(int csfd, int uffd);
      void removeServiceThread(int csfd);
      void stop_service_threads();
      //End of Locking Interfaces
  };

  void start_umap_service(int csfd);
}
