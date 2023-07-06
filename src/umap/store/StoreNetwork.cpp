//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cassert>

#include "StoreNetwork.h"
#include "umap/store/Store.hpp"
#include "umap/util/Macros.hpp"
#include "umap/RegionManager.hpp"

namespace Umap {

  NetworkEndpoint::NetworkEndpoint(){}

  int NetworkEndpoint::setup_ib_common()
  {
    struct ibv_device **dev_list, *ib_dev;

    srand48(getpid() * time(NULL));

    // Get an IB devices list:
    dev_list = ibv_get_device_list(NULL);
    if (!dev_list)
    {
        perror("Failed to get IB devices list.\n");
        return 1;
    }

    // Get an IB device:
    ib_dev = *dev_list;
    if (!ib_dev)
    {
        perror("No IB devices found.\n");
        return 1;
    }

    metabuf_size = IB_METABUFFER_SIZE;
    ib_buf = malloc(roundup(IB_METABUFFER_SIZE, sysconf(_SC_PAGESIZE)));
    if (!ib_buf) {
        perror("Couldn't allocate ib_buf.\n");
        return 1;
    }
    memset(ib_buf, 0, IB_METABUFFER_SIZE);

    // Open an IB device context:
    ctx = ibv_open_device(*dev_list);
    if (!ctx)
    {
      fprintf(stderr, "Couldn't get context for %s.\n", ibv_get_device_name(ib_dev));
      return 1;
    }

    // Allocate a Protection Domain:
    pd = ibv_alloc_pd(ctx);
    if (!pd)
    {
      perror("Failed to allocate Protection Domain.\n");
      return 1;
    }

    // Query IB port attribute
    memset(&port_info, 0, sizeof(port_info));
    if(ibv_query_port(ctx, IB_PORT, &port_info))
    {
      perror("Failed to query IB port information.\n");
      return 1;
    }

    // Query Device attribute
    struct ibv_device_attr device_attr;
    if (ibv_query_device(ctx, &device_attr))
    {
        perror("Failed to query IB device information.\n");
        return 1;
    }else{
        printf("The maximum number of QP = %d\n", device_attr.max_qp);
        printf("Largest contiguous block that can be registered %llu\n", device_attr.max_mr_size);
        printf("Maximum number of outstanding WR = %d\n", device_attr.max_qp_wr);
    }

    // Register a Memory Region for communicating metadata
    mr = ibv_reg_mr(pd, ib_buf, metabuf_size,
                           IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
    if (!mr)
    {
        perror("Couldn't register Memory Region.\n");
        return 1;
    }

    // Creates a Completion Queue
    cq = ibv_create_cq(ctx, COUNT, NULL, NULL, 0);
    if (!cq)
    {
      perror("Couldn't create Completion Queue.\n");
      return 1;
    }

    // Creates a Queue Pair:
    {
      struct ibv_qp_init_attr qp_init_attr = {
              .send_cq = cq,
              .recv_cq = cq,
              .cap = {
                      .max_send_wr = COUNT,
                      .max_recv_wr = COUNT,
                      .max_send_sge = 1,
                      .max_recv_sge = 1,
              },
              .qp_type = IBV_QPT_RC,
      };

      qp = ibv_create_qp(pd, &qp_init_attr);
      if (!qp) {
          perror("Couldn't create Queue Pair.\n");
          return 1;
      }

      struct ibv_qp_attr qp_attr;
      memset(&qp_attr, 0, sizeof(qp_attr));
      qp_attr.qp_state        = IBV_QPS_INIT;
      qp_attr.pkey_index      = 0;
      qp_attr.port_num        = IB_PORT;
      qp_attr.qp_access_flags = IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

      if (ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS)) {
          perror("Failed to modify QP to INIT.\n");
          return 1;
      }
    }

    // Get LID:
    local_dest.lid = port_info.lid;
    if ( port_info.link_layer == IBV_LINK_LAYER_INFINIBAND && !local_dest.lid)
    {
        perror("Couldn't get LID.\n");
        return 1;
    }

    // Get QPN:
    local_dest.qpn = qp->qp_num;

    // Set PSN:
    local_dest.psn = lrand48() & 0xffffff;

    return 0;
  }

  int NetworkEndpoint::connect_between_qps()
  {
    struct ibv_qp_attr qp_attr;
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.qp_state		= IBV_QPS_RTR;
    qp_attr.path_mtu		= MTU;
    qp_attr.dest_qp_num	= remote_dest.qpn;
    qp_attr.rq_psn			= remote_dest.psn;
    qp_attr.max_dest_rd_atomic	= 1;
    qp_attr.min_rnr_timer		    = 12;
    qp_attr.ah_attr.is_global	  = 0;
    qp_attr.ah_attr.dlid  = remote_dest.lid;
    qp_attr.ah_attr.sl		= 0;
    qp_attr.ah_attr.src_path_bits	= 0;
    qp_attr.ah_attr.port_num	= IB_PORT;


    if (ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN |
                                    IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER | IBV_QP_AV)) {
        perror("Failed to modify QP to RTR.\n");
        return 1;
    }

    qp_attr.qp_state	    = IBV_QPS_RTS;
    qp_attr.timeout	      = 14;
    qp_attr.retry_cnt	    = 7;
    qp_attr.rnr_retry	    = 7;
    qp_attr.sq_psn	      = local_dest.psn;
    qp_attr.max_rd_atomic = 1;
    if (ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
                                    IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC))
    {
        perror("Failed to modify QP to RTS.\n");
        return 1;
    }

    return 0;
  }

  void NetworkEndpoint::close_ib_connection(){
    
    if (qp)
    {
        ibv_destroy_qp(qp);
    }

    if (cq)
    {
        ibv_destroy_cq(cq);
    }

    if (mr)
    {
        ibv_dereg_mr(mr);
    }

    if (pd)
    {
        ibv_dealloc_pd(pd);
    }

    if (ctx)
    {
        ibv_close_device(ctx);
    }

    if (ib_buf)
    {
        free(ib_buf);
    }

  } 

  int  NetworkEndpoint::post_recv(int size)
  {
    memset(ib_buf, 0, IB_METABUFFER_SIZE);
    struct ibv_sge list = {
      .addr	  = (uint64_t)ib_buf,
      .length = (uint32_t)size,
      .lkey	= mr->lkey
    };

    struct ibv_recv_wr *bad_wr;
    struct ibv_recv_wr wr;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id	    = RECV_WRID;
    wr.sg_list    = &list;
    wr.num_sge    = 1;
    wr.next       = NULL;

    int res = ibv_post_recv(qp, &wr, &bad_wr);
    return res;

  }

  int  NetworkEndpoint::post_send(int size)
  {
    struct ibv_sge list = {
      .addr	  = (uint64_t)ib_buf,
      .length = (uint32_t)size,
      .lkey	  = mr->lkey
    };

    struct ibv_send_wr *bad_wr;
    struct ibv_send_wr wr;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id	    = SEND_WRID;
    wr.sg_list    = &list;
    wr.num_sge    = 1;
    wr.opcode     = IBV_WR_SEND;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.next       = NULL;

    return ibv_post_send(qp, &wr, &bad_wr);
  }

  int  NetworkEndpoint::wait_completions(uint64_t wr_id){return 0;}

  int  NetworkServer::wait_completions(uint64_t wr_id)
  { // only one thread (the server) will call this
    UMAP_LOG(Info, "wr_id:"<<wr_id);
    int finished = 0, count = 1;
    while (finished < count)
    {
      struct ibv_wc wc[WC_BATCH];
      int n;
      do {
          n = ibv_poll_cq(cq, WC_BATCH, wc);
          if (n < 0)
          {
              fprintf(stderr, "Poll CQ failed %d\n", n);
              return 1;
          }
      } while (n < 1);

      for (int i = 0; i < n; i++)
      {
        if (wc[i].status != IBV_WC_SUCCESS)
        {
          fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
                  ibv_wc_status_str(wc[i].status), wc[i].status, (int)wc[i].wr_id);
          return 1;
        }

        if(wc[i].wr_id == wr_id)
          finished++;
      }
    }
    UMAP_LOG(Info, "wr_id:"<<wr_id);
    return 0;
  }

  int  NetworkClient::wait_completions(uint64_t wr_id)
  { // multiple filler and evictor may call this concurrently
    UMAP_LOG(Info, "wr_id:"<<wr_id);
    int finished = 0, count = 1;
    while (finished < count)
    {
      struct ibv_wc wc[WC_BATCH];
      int n;
      do {
          n = ibv_poll_cq(cq, WC_BATCH, wc);
          if (n < 0)
          {
              fprintf(stderr, "Poll CQ failed %d\n", n);
              return 1;
          }
      } while (n < 1);

      for (int i = 0; i < n; i++)
      {
        if (wc[i].status != IBV_WC_SUCCESS)
        {
          fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
                  ibv_wc_status_str(wc[i].status), wc[i].status, (int)wc[i].wr_id);
          return 1;
        }
        UMAP_LOG(Info, "wc["<<i<<"].wr_id"<<wc[i].wr_id);
        if(wc[i].wr_id == wr_id)
          finished++;
      }
    }

    UMAP_LOG(Info, "wr_id:"<<wr_id);
    return 0;
  }

  NetworkServer::~NetworkServer(){
    //todo
  }
  NetworkServer::NetworkServer(){

    int res = setup_ib_common();
    if( res ) UMAP_ERROR(" failed to setup IB");
    
    res = get_client_dest();
    if( res ) UMAP_ERROR(" failed to get_client_dest");

    res = connect_between_qps();
    if( res ) UMAP_ERROR(" failed to connect_between_qps");
    
  }

  int NetworkServer::get_client_dest()
  {
    int sockfd, connfd;
    struct sockaddr_in server_address, client_address;

    // Start connection:
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
      perror("Socket creation failed!\n");
      return 1;
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PORT);

    if(bind(sockfd, (struct sockaddr*)&server_address, sizeof(server_address)))
    {
      perror("Socket bind failed!\n");
      return 1;
    }

    if(listen(sockfd, 5))
    {
      perror("Listen failed...\n");
      return 1;
    }

    socklen_t len = (socklen_t) sizeof(client_address);

    connfd = accept(sockfd, (struct sockaddr*)&client_address, &len);
    if(connfd < 0)
    {
      perror("Server accept failed!\n");
      return 1;
    }

    // Get client details and then send my details
    {
      char *buf;
      buf = (char*)malloc(sizeof(struct IBDest));
      int offset = 0;
      while (offset < sizeof(struct IBDest))
          offset += read(connfd, buf + offset, sizeof(struct IBDest) - offset);

      memcpy(&remote_dest, buf, sizeof(struct IBDest));

      // Send my details:
      memset(buf, 0, sizeof(struct IBDest));
      offset = 0;
      memcpy(buf, &local_dest, sizeof(struct IBDest));
      write(connfd, buf, sizeof(struct IBDest));
    }

    // Finish connection:
    close(sockfd);

    return 0;
  }

  void NetworkServer::wait_till_disconnect()
  {
    bool done = false;
    while( !done ){
      post_recv(sizeof(int)*2);
      wait_completions(RECV_WRID);
      int *buf = (int*)ib_buf;
      int num_pages = buf[0];
      //memcpy(&num_pages, ib_buf, sizeof(int));
      printf("Received num_pages=%d\n", num_pages);

      //Check if it is the signal for closing IB connection
      if(num_pages == 0){
        done = true;
        break;        
      }

      int page_size = buf[1];
      printf("Received page_size=%d\n", page_size);

      //struct ibv_mr **mrs = (struct ibv_mr **)malloc( sizeof(struct ibv_mr *)*num_pages );
      //struct RemoteMR *remote_mrs = (struct RemoteMR *) malloc(sizeof(struct RemoteMR)*num_pages ); 
      struct RemoteMR *remote_mrs = (struct RemoteMR *) ib_buf;
      //todo: need to check buffer size
      size_t size = num_pages * page_size;

      //Option 1: allocate memory
      buf = (int*) malloc(size);
      memset(buf, 0, size);
      printf("Allocated %zu\n", size);   

      //Register remote region one by one for each page
      char* p_addr = (char* ) buf;
      for(int i=0; i<num_pages; i++)
      {
          struct ibv_mr *mr = ibv_reg_mr(pd, p_addr, page_size,
                          IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE); // need IBV_ACCESS_LOCAL_WRITE or it fails      
          if (!mr)
          {
              perror("Couldn't register Memory Region\n");
              return;
          }
          printf("Registered Page %zu\n", i);
          mem_regions.push_back(mr);
          remote_mrs->remote_addr = (uint64_t) p_addr;
          remote_mrs->rkey = mr->rkey;
          p_addr += page_size;
          remote_mrs ++;
      }
      printf("Finished registering %zu pages\n", num_pages);      

      //Send remote address and rkey
      //memcpy(ib_buf, remote_mrs, sizeof(struct RemoteMR)*num_pages );
      post_send(sizeof(struct RemoteMR)*num_pages);
      wait_completions(SEND_WRID);
      printf("Finished sending %d remote regions to client\n", num_pages);
    }

    //TODO
    //add free up
    //deregistering all MRs
    UMAP_LOG(Info, "Server de-registering memory regions");
    size_t num_memory_regions = mem_regions.size();
    for(int i=0; i<num_memory_regions; i++)
    {
      if (ibv_dereg_mr(mem_regions[i])) {
        fprintf(stderr, "Error, ibv_dereg_mr() failed for %d-th region\n", i);
      }
    }
    UMAP_LOG(Info, "Server close IB connection");
    double* buf = (double*) mem_regions[0]->addr;
    printf("Server local buffer %f \n", buf[0]);
    buf = (double*) mem_regions[1]->addr;
    printf("Server local buffer %f \n", buf[0]);
    buf = (double*) mem_regions[2]->addr;
    printf("Server local buffer %f \n", buf[0]);
    close_ib_connection();
    UMAP_LOG(Info, "Server shutdown");
  }

  NetworkClient::NetworkClient( const char* _server_name_ ){
  
    memset(server_name, '\0', sizeof(server_name));
    strcpy(server_name, _server_name_);
    int res = setup_ib_common();
    if( res ) UMAP_ERROR(" failed to setup IB");

    res = get_server_dest();
    if( res ) UMAP_ERROR(" failed to get_server_dest");
    
    res = connect_between_qps();
    if( res ) UMAP_ERROR(" failed to connect_between_qps");
  }
  
  NetworkClient::~NetworkClient(){
    UMAP_LOG(Info, "destroying Client");
    // inform the server to shut down
    int signal_end = 0;
    memcpy(ib_buf, &signal_end, sizeof(int));
    post_send(sizeof(int)); 
    wait_completions(SEND_WRID);

    close_ib_connection();
  }
  
  int NetworkClient::get_server_dest()
  {
    int sockfd;
    struct addrinfo hints, *result, *rp;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6.
    hints.ai_socktype = SOCK_STREAM; // TCP

    if(getaddrinfo(server_name, PORT_STR, &hints, &result))
    {
      perror("Found address failed!\n");
      return 1;
    }

    // Start connection:
    for(rp = result; rp; rp = rp->ai_next)
    {
      sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

      if(sockfd == -1)
          continue;

      if(!connect(sockfd, rp->ai_addr, rp->ai_addrlen))
          break; // Success.

      close(sockfd);
    }

    if(!rp)
    {
      perror("Connection with the server failed.\n");
      return 1;
    }

    freeaddrinfo(result);

    // Get server details:
    {
      char *buf;
      buf = (char *)malloc(sizeof(struct IBDest));
      int offset = 0;

      // Send my details:
      memcpy(buf, &local_dest, sizeof(struct IBDest));
      write(sockfd, buf, sizeof(struct IBDest));

      // Receive server's details:
      memset(buf, 0, sizeof(struct IBDest));
      offset = 0;
      while (offset < sizeof(struct IBDest))
          offset += read(sockfd, buf + offset, sizeof(struct IBDest) - offset);

      memcpy(&remote_dest, buf, sizeof(struct IBDest));
    }

    // Finish connection:
    close(sockfd);

    return 0;
  }

  StoreNetwork::StoreNetwork(const char* _region_, size_t _rsize_, NetworkEndpoint* _endpoint_)
    : rsize{_rsize_}, alignsize{Umap::RegionManager::getInstance().get_umap_page_size()}
  {
    region_name = std::string(_region_);
    UMAP_LOG(Info, "region_name: " << region_name << " rsize: " << rsize
              << " alignsize: " << alignsize );

    endpoint = dynamic_cast<NetworkClient*>(_endpoint_);

    // Notify the server the number of pages to allocate
    if( rsize % alignsize != 0 )
      UMAP_ERROR(" region size " << rsize << " is not divisible by " << alignsize );
    int *buf = (int*) endpoint->get_buf();
    int num_pages = rsize/alignsize;
    buf[0] = num_pages;
    buf[1] = alignsize;
    //memcpy(endpoint->get_buf(), &num_pages, sizeof(int));
    endpoint->post_send(sizeof(int)*2); 
    endpoint->wait_completions(SEND_WRID);

    // Get Server's remote address and rkey
    //remote_mrs = (struct RemoteMR *) malloc(sizeof(struct RemoteMR)*num_pages ); 
    endpoint->post_recv(sizeof(struct RemoteMR)*num_pages);
    endpoint->wait_completions(RECV_WRID);
    //memcpy(remote_mrs, ib_buf, sizeof(struct RemoteMR)*num_pages);
    struct RemoteMR *remote_mr_ptr = (struct RemoteMR *) endpoint->get_buf();
    printf("Client received RemoteMR for %d pages\n", num_pages);

    for(int i=0; i<num_pages; i++ ){
      remote_mrs.push_back(*remote_mr_ptr);
      remote_mr_ptr ++;
      printf("remote address %lu, rkey = %zu\n", remote_mrs.back().remote_addr, remote_mrs.back().rkey);
    }
    void*   local_send_ptr = malloc(alignsize);
    local_send_mr  = ibv_reg_mr(endpoint->get_pd(), local_send_ptr, alignsize, IBV_ACCESS_LOCAL_WRITE);
    if (!local_send_mr)
    {
        UMAP_ERROR("Could not register local_send_mr");
    }
  }

  int StoreNetwork::create_local_region(char* buf, size_t nb, int mode)
  {
    ibv_mr *mr;
    if( mode == 0 )
      mr = ibv_reg_mr(endpoint->get_pd(), buf, nb,
                    IBV_ACCESS_LOCAL_WRITE); // | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE
    else
      mr = ibv_reg_mr(endpoint->get_pd(), buf, nb,
                    IBV_ACCESS_LOCAL_WRITE);

    if (!mr)
    {
      printf("Couldn't register Memory Region for buffer %p\n", buf);
      return 1;
    }else{
      printf("Registered buffer %p in create_local_region\n", buf);
      local_mrs_map[(uint64_t)buf] = mr;
    }
    return 0;
  }

  ssize_t StoreNetwork::read_from_store(char* buf, size_t nb, off_t off)
  {
    assert( off % alignsize == 0);
    assert( nb == alignsize);
    UMAP_LOG(Info, region_name << " off " << off);

    int rval = 0;

    if( local_mrs_map.find((uint64_t)buf) == local_mrs_map.end())
    {
        rval = create_local_region( buf, nb, 0);
        if( rval ) 
          UMAP_ERROR("Failed to create local region for buffer " << (uint64_t)buf );
    }
    ibv_mr *local_mr = local_mrs_map[(uint64_t)buf];

    int page_id = off / alignsize;

    struct ibv_sge list = {
      .addr	  = (uint64_t)buf,
      .length = (uint32_t) nb,
      .lkey	  = local_mr->lkey
    };

    struct ibv_send_wr *bad_wr;
    struct ibv_send_wr wr_read;
    memset(&wr_read, 0, sizeof(wr_read));
    uint64_t wr_id      = remote_mrs[page_id].remote_addr + (uint64_t)off;
    wr_read.wr_id	     = wr_id;
    wr_read.sg_list    = &list;
    wr_read.num_sge    = 1;
    wr_read.opcode     = IBV_WR_RDMA_READ;
    wr_read.send_flags = IBV_SEND_SIGNALED;
    wr_read.wr.rdma.remote_addr =  remote_mrs[page_id].remote_addr;
    wr_read.wr.rdma.rkey = remote_mrs[page_id].rkey;
    wr_read.next         = NULL;

    rval = ibv_post_send(endpoint->get_qp(), &wr_read, &bad_wr);
    if( rval ) 
          UMAP_ERROR("Failed to send request request " );

    rval = endpoint->wait_completions(wr_id);
    if( rval ) 
          UMAP_ERROR("Failed to wait_completions ");    

    return rval;
  }

  ssize_t  StoreNetwork::write_to_store(char* buf, size_t nb, off_t off)
  {
    assert( off % alignsize == 0);
    assert( nb == alignsize);
    UMAP_LOG(Info, region_name << " off " << off);

    int rval = 0;
    memcpy(local_send_mr->addr, buf, nb);

    int page_id = off / alignsize;

    struct ibv_sge list = {
      .addr	  = (uint64_t) local_send_mr->addr,
      .length = (uint32_t) nb,
      .lkey	  = local_send_mr->lkey
    };

    struct ibv_send_wr *bad_wr;
    struct ibv_send_wr wr_write;
    memset(&wr_write, 0, sizeof(wr_write));
    uint64_t wr_id      = remote_mrs[page_id].remote_addr + (uint64_t)off;
    wr_write.wr_id	    = wr_id;
    wr_write.sg_list    = &list;
    wr_write.num_sge    = 1;
    wr_write.opcode     = IBV_WR_RDMA_WRITE;
    wr_write.send_flags = IBV_SEND_SIGNALED;
    wr_write.wr.rdma.remote_addr = remote_mrs[page_id].remote_addr;
    wr_write.wr.rdma.rkey = remote_mrs[page_id].rkey;
    wr_write.next         = NULL;

    rval = ibv_post_send(endpoint->get_qp(), &wr_write, &bad_wr);
    if( rval ) 
      UMAP_ERROR("Failed to ibv_post_send " << (uint64_t)buf );

    rval = endpoint->wait_completions(wr_id);
    if( rval ) 
      UMAP_ERROR("Failed to wait_completions WRITE_WRID " << wr_id);

    return rval;
  }
}
