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

#define NAME_SIZE 100
#define UMAP_SERVER_PATH "/tmp/umap-server"

namespace Umap{

  class UmapServerManager;
  class UmapServiceThread;

  enum class uffd_actions: int{
    umap,
    unmap,
  };

  typedef struct{
    uint64_t size;
    void *base_addr;
  }region_loc;
 
  typedef struct {
    int prot;
    int flags;
  }umap_file_params;

  typedef struct{
    uffd_actions act;
    char name[NAME_SIZE];
    umap_file_params args;
  }ActionParam;

  class UmapServInfo{
    friend class ClientManager;
    private:
      int memfd;
      std::string filename;
      umap_file_params args; 
      region_loc loc;
      int umap_server_fd;

      int setup_remote_umap_handle();
      void remove_remote_umap_handle();
    public:
      UmapServInfo(int sfd, std::string fname, umap_file_params a):umap_server_fd(sfd),filename(fname),args(a){
        setup_remote_umap_handle();
      }
      ~UmapServInfo(){
        remove_remote_umap_handle();
      }
  }; 
  class ClientManager{
    private:
      std::mutex cm_mutex;
      std::string umap_server_path;
      int umap_server_fd;
      static ClientManager *instance;
      std::map<std::string, UmapServInfo*> file_conn_map; 
      
      ClientManager(std::string server_path){
        umap_server_path = server_path;
        umap_server_fd = 0; 
      }

      ~ClientManager(){}
      UmapServInfo *cs_umap(std::string filename, int, int);
      void cs_uunmap(std::string filename);

    public:
      static ClientManager *getInstance(std::string server_path=std::string()){
        if(!instance){
          if(!server_path.empty())
            instance = new ClientManager(server_path);
          else
            UMAP_ERROR("Server Path can't be empty during init");
        }
        return instance;
      }

      static void deleteInstance(){
        if(!instance){
          UMAP_ERROR("ClientManager instance does not exist");
        }else{
          delete instance;
        }
      }
      //Start Interface that need to lock: synchronizes requests from multiple threads
      void setupUmapConnection();
      void closeUmapConnection();
      void *map_req(std::string filename, int prot, int flags);
      int unmap_req(std::string filename);
      //End of interfaces that lock
  };


  class mappedRegionInfo{
    friend class UmapServiceThread;
    private:
      int memfd;
      region_loc reg;
      region_loc remote_reg;
      int filefd;
    public:
       mappedRegionInfo(int fd, int mfd, void *b, uint64_t l):filefd(fd), memfd(mfd){ reg.base_addr = b; reg.size = l;}
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
