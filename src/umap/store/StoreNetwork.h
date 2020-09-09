//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

#ifndef _UMAP_NETWORK_STORE_H_
#define _UMAP_NETWORK_STORE_H_
//#include <cstdint>
#include "umap/store/Store.hpp"
#include "umap/umap.h"


namespace Umap {

  class StoreNetwork : public Store {
  public:
    StoreNetwork(const char* _id, std::size_t _rsize_, bool _is_server=false);
    virtual ~StoreNetwork();
    std::size_t get_size(){return rsize;}
    
    ssize_t read_from_store(char* buf, size_t nb, off_t off);
    ssize_t write_to_store(char* buf, size_t nb, off_t off);

  protected:
    /* per-data object attributes*/
    const char* id;
    size_t rsize;
    bool is_on_server;
    size_t num_clients;
  };

  class StoreNetworkServer : public StoreNetwork {
  public:
    StoreNetworkServer(const char* _id, void* _ptr, std::size_t _rsize_, std::size_t _num_clients=0);
    ~StoreNetworkServer();

  private:
    int server_id;

  };
  
  class StoreNetworkClient : public StoreNetwork {
  public:
    StoreNetworkClient(const char* id, std::size_t _rsize_);
    ~StoreNetworkClient();

  private:
    int client_id;

  };
}
#endif
