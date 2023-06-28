//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_STORE_NETWORK_H_
#define _UMAP_STORE_NETWORK_H_
#include <cstdint>
#include <map>
#include <vector>
#include "umap/store/Store.hpp"
#include "umap/umap.h"
#include <infiniband/verbs.h>

#define PORT 8080
#define PORT_STR "8080"

#define IB_PORT 1
#define MTU IBV_MTU_4096
#define COUNT 1000
#define WC_BATCH 100
#define IB_METABUFFER_SIZE 32768

enum {
  RECV_WRID = 1,
  SEND_WRID = 2,
  READ_WRID = 4,
  WRITE_WRID = 8,
};

namespace Umap {
  class NetworkEndpoint{
      /*
      struct ibv_mr {
        struct ibv_context     *context;
        struct ibv_pd	       *pd;
        void		       *addr;
        size_t			length;
        uint32_t		handle;
        uint32_t		lkey;
        uint32_t		rkey;
      };
      */
      struct RemoteMR {
        uint64_t remote_addr;
        uint32_t rkey;
      };
      
      struct IBRes {
        struct ibv_context *ctx;
        struct ibv_pd *pd;
        struct ibv_mr *mr;
        struct ibv_cq *cq;
        struct ibv_qp *qp;
        struct ibv_port_attr port_info;

        void *buf;
        size_t size;
      };

      struct IBDest {
        int lid;
        int qpn;
        int psn;
      };

    public:
        NetworkEndpoint();
        void close_ib_connection();
        int wait_completions(int wr_id);
        struct ibv_qp* get_qp(){return qp;}
        struct ibv_pd* get_pd(){return pd;}

    protected:
        int setup_ib_common();
        int connect_between_qps();

        int post_recv(int size);
        int post_send(int size);

    private:
        struct IBDest local_dest;
        struct IBDest remote_dest;

        struct ibv_context *ctx;
        struct ibv_pd *pd;
        struct ibv_mr *mr;
        struct ibv_cq *cq;
        struct ibv_qp *qp;
        struct ibv_port_attr port_info;
        void       *metabuf;
        size_t metabuf_size;

  };

  class NetworkServer: public NetworkEndpoint{
    public:
      NetworkServer();
      void wait_till_disconnect();

    private:
      int get_client_dest();
      std::vector<struct ibv_mr *> mem_regions;

  };

  class NetworkClient: public NetworkEndpoint{
    public:
      NetworkClient( char* _server_name_  );
    private:
      int get_server_dest();
      char server_name[64];
  };

  class StoreNetwork : public Store {

    public:
      StoreNetwork(const void* _region_, size_t _rsize_, size_t _alignsize_, NetworkEndpoint* _endpoint_);

      ssize_t read_from_store(char* buf, size_t nb, off_t off);
      ssize_t  write_to_store(char* buf, size_t nb, off_t off);
    private:
      void* region;
      void* alignment_buffer;
      size_t rsize;
      size_t alignsize;
      NetworkEndpoint* endpoint;
      std::vector<struct ibv_mr *> remote_mrs;
      std::map<uint64_t, ibv_mr *> local_mrs_map;
      void create_local_region(char* buf, size_t nb)
  };
}
#endif
