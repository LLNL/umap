#include <string>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <unistd.h>
#include <poll.h>
#include "UmapServiceCommon.hpp"
#include "umap-client.h"

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
        else{
          std::cout<<"Umap-Client Error: Server Path can't be empty during init"<<std::endl;
          exit(-1);
	}
      }
      return instance;
    }

    static void deleteInstance(){
      if(!instance){
        std::cout<<"Umap-Client Error: ClientManager instance does not exist"<<std::endl;
        exit(-1);
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
