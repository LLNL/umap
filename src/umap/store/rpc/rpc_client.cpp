//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include <cassert>
#include <mpi.h>

#include "umap/util/Macros.hpp"
#include "rpc_client.hpp"
#include "rpc_util.hpp"

static margo_instance_id mid;
static hg_id_t umap_request_rpc_id;
static hg_id_t umap_release_rpc_id;
static hg_id_t umap_read_rpc_id;
static hg_id_t umap_write_rpc_id;

static ClientResourcePool resource_pool;
static int client_id=-1;
static std::map<int, char*> server_str_map;
static std::map<int, hg_addr_t> server_map;

size_t client_get_resource_size(const char*id)
{
  assert(resource_pool.find(id)!=resource_pool.end());
  return resource_pool[id].rsize;
}

void print_client_memory_pool()
{
  for(auto it : resource_pool){
    UMAP_LOG(Info, "Client "<< client_id
	     <<" pool[ " << it.first << " ] :: "
	     <<"size=" << (it.second).rsize
	     << ", server_stride=" << (it.second).server_stride);

    for(auto it2 : (it.second).meta_table )
      UMAP_LOG(Info, "Server "<< it2.server_id << " : "<< it2.offset << ", " <<it2.size) ;
  }
}

/* Validate no duplicate remote memory object has been registered the pool */
bool client_check_resource(const char* id){

  if( resource_pool.find(id)!=resource_pool.end() ){
    return false;
  }
  
  return true;
}


/* Send request to the servers to request rsize bytes */
/* If rsize=0 the size is return by the servers */
/* a blocking operation returns */
/* when the response from server arrives */
void client_request_resource(const char* id, size_t requested_size){
  
  size_t num_servers = server_map.size();
  assert(num_servers>0);

  /* Create input structure */
  umap_request_rpc_in_t in;
  in.id = strdup(id);
  in.size = 0; //ask servers to return their size

  /* send request to the server list*/
  hg_handle_t handle_list[num_servers];
  int i=0;
  for(auto it : server_map){
    
    int server_id = it.first;
    hg_addr_t server_address = it.second;
  
    /* Create a RPC handle */
    hg_return_t ret;
    ret = margo_create(mid,
		       server_address,
		       umap_request_rpc_id,
		       &(handle_list[i]));
    assert(ret == HG_SUCCESS);
  
    /* Forward RPC requst to the server */
    ret = margo_forward(handle_list[i], &in);
    assert(ret == HG_SUCCESS);

    i++;
  }

  
  /* verify the response from all servers */
  size_t offset=0;
  for( i=0;i<num_servers;i++ ){
        
    /* Create output structure */
    umap_request_rpc_out_t out;
    hg_return_t ret;
    ret = margo_get_output(handle_list[i], &out);
    assert(ret == HG_SUCCESS);

    if( out.ret==RPC_RESPONSE_REQ_SIZE){
      // this server can provide the (partial) resource, register it
      // TODO: assume offset if order of server id, to be extended
      resource_pool[id].meta_table.emplace_back(offset, out.size, i);
      offset += out.size;

      //UMAP_LOG(Info, "Server "<< i <<"/"<<num_servers<<" return size "<< out.size << " for " << id << " total_size="<<offset);

    }else{
      // if this server cannot provide the resource, skip it
      if( out.ret==RPC_RESPONSE_REQ_UNAVAIL){
	UMAP_LOG(Warning, "The requested "<< id <<" is unavailable on Server "<<i);
      }else if( out.ret==RPC_RESPONSE_REQ_WRONG_SIZE){
	UMAP_LOG(Warning, "The requested "<< id <<" has mismatched size on Server "<<i);
      }else{
	UMAP_LOG(Warning, "Unrecognized return message from Server "<<i);
      }
    }

    /* Free output structure */
    margo_free_output(handle_list[i], &out);

    /* Free handle handles*/
    ret = margo_destroy(handle_list[i]);
    assert(ret == HG_SUCCESS);    
  }
  
  if( offset<requested_size ){
    UMAP_LOG(Warning, "Client requested " << requested_size <<
	              " bytes, but only " << offset << " bytes are available on all servers");
  }
  
  // update the meta data if accepted
  resource_pool[id].num_servers=resource_pool[id].meta_table.size();
  resource_pool[id].rsize=offset;
  resource_pool[id].server_stride = 0;
  if(offset>0){
    size_t stride = resource_pool[id].meta_table[0].size;
    for(auto it : resource_pool[id].meta_table){
      if(it.size!=stride){
	stride=0;
	break;
      }
    }
    resource_pool[id].server_stride = stride;
  }

}

/* Register the remote memory to the pool */
bool client_add_resource(const char*id,
			 size_t rsize)
{
  // try to add the resource locally
  resource_pool.emplace(id, RemoteResource(rsize, server_map.size()));

  // send request to all servers
  client_request_resource(id, rsize);

  // currently only accept if the size on server is equal or larger than the requested
  // it might be relaxed
  if( resource_pool[id].rsize<rsize ){
    resource_pool.erase(id);    
    return false;  
  }

  //print_client_memory_pool();

  return true;
}

/* Delete the memory resource from the pool on the client */
/* Inform the server about the release of the resource */
/* TODO: A blocking operation that returns when response arrives */
int client_release_resource(const char* id){

  int ret = 0;
  
  if( resource_pool.find(id)==resource_pool.end() ){
    UMAP_ERROR(id<<" is not found in the pool");
    return -1;
  }

  /* Start informing the server */
  RemoteResource &obj = resource_pool[id];
  std::vector<int> server_id_list;
  
  if(obj.server_stride>0){
    for(auto s=0; s<obj.num_servers; s++)
      server_id_list.push_back(s);
  }else{
    std::vector<ServerMetadata>& metatable= obj.meta_table;
    for(auto it : metatable){
      server_id_list.push_back(it.server_id);
    }
  }

  for(auto server_id : server_id_list){
    auto it = server_map.find(server_id);
    assert( it!=server_map.end());  
    hg_addr_t server_address = it->second;
  
    /* Create a RPC handle */
    hg_return_t hret;
    hg_handle_t handle;
    hret = margo_create(mid,
			server_address,
			umap_release_rpc_id,
			&handle);
    assert(hret == HG_SUCCESS);

    /* Create input structure */
    umap_release_rpc_in_t in;
    in.id = strdup(id);
  
    /* Forward RPC requst to the server */
    hret = margo_forward(handle, &in);
    assert(hret == HG_SUCCESS);
    
    /* verify the response */
    /* TODO: should the client wait for the reponse? */
    umap_release_rpc_out_t out;
    hret = margo_get_output(handle, &out);
    assert(hret == HG_SUCCESS);
    assert( out.ret==RPC_RESPONSE_RELEASE);
  } 
  /* End of informing the server*/

  resource_pool.erase(id);
  
  /* TODO: shutdown */
  if(resource_pool.size()==0){
    UMAP_LOG(Info, "shuting down Client " << client_id);
    client_fini();
  }
  
  return ret;
}


/* Read the server address published in the file */
/* Record the servers into the map */
static void get_server_address_string(){

  FILE* fp = fopen(LOCAL_RPC_ADDR_FILE, "r");

  /* read server address from local file */
  if (fp != NULL) {
    int server_id =0;
    char addr_string[MAX_ADDR_LENGTH];
    int r = fscanf(fp, "%s\n", addr_string);
    while(r==1){
      server_str_map[server_id]=strdup(addr_string);
      server_id ++;
      memset(addr_string, 0, MAX_ADDR_LENGTH);
      r = fscanf(fp, "%s\n", addr_string);
      UMAP_LOG(Info, "server "<<(server_id-1)<<" "<<server_str_map[server_id-1] );
    }
    assert(r==EOF);
    fclose(fp);
  }else
    UMAP_ERROR("Unable to find local server rpc address ");
}

static void setup_margo_client(){

  /* get the protocol used by the server */
  get_server_address_string();
  assert(server_str_map.size()>0);

  char* server_address_string = server_str_map[0];
  char* protocol = strdup(server_address_string);
  char* del = strchr(protocol, ';');
  if (del) *del = '\0';
  UMAP_LOG(Info, "server :"<<server_address_string<<" protocol: "<<protocol);
  
  /* Init Margo using server's protocol */
  int use_progress_thread = 1;//flag to use a dedicated thread for running Mercury's progress loop. 
  int rpc_thread_count = 1; //number of threads for running rpc calls
  mid = margo_init(protocol, MARGO_CLIENT_MODE, use_progress_thread, rpc_thread_count);
  free(protocol);
  if (mid == MARGO_INSTANCE_NULL) {
    free(server_address_string);
    UMAP_ERROR("margo_init protocol failed");
  }

  
  /* TODO: want to keep this enabled all the time */
  margo_diag_start(mid);

  
  /* lookup server address from string */
  for(auto it : server_str_map ){
    int server_id = it.first;
    char* addr_str = strdup(it.second);
    hg_addr_t server_address = HG_ADDR_NULL;
    margo_addr_lookup(mid, addr_str, &server_address);
    if (server_address == HG_ADDR_NULL) {
      margo_finalize(mid);
      UMAP_ERROR("Failed to lookup margo server address from string: "<<addr_str);
    }
    server_map[server_id]=server_address;
  }
  //UMAP_LOG(Info, "margo_init done");
  
  /* Find the address of this client process */
  hg_addr_t client_address;
  hg_return_t ret = margo_addr_self(mid, &client_address);
  if (ret != HG_SUCCESS) {
    margo_finalize(mid);
    UMAP_ERROR("failed to lookup margo_addr_self on client");
  }

  /* Convert the address to string*/
  char client_address_string[128];
  hg_size_t len = sizeof(client_address_string);
  ret = margo_addr_to_string(mid, client_address_string, &len, client_address);
  if (ret != HG_SUCCESS) {
    margo_addr_free(mid, client_address);
    margo_finalize(mid);
    UMAP_ERROR("failed to convert client address to string");
  }
  UMAP_LOG(Info, "Margo client adress: " << client_address_string);
  
}


/*
 * Initialize a margo client on the calling process
 */
void client_init(void)
{
  
  /* setup Margo RPC only if not done */
  if( mid != MARGO_INSTANCE_NULL ){
    UMAP_ERROR("Clients have been initialized before, returning...");
  }else{
    
    /* bootstraping to determine server and clients usnig MPI */
    /* not needed if MPI protocol is not used */
    int flag_mpi_initialized;
    MPI_Initialized(&flag_mpi_initialized);
    if( !flag_mpi_initialized )
      MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &client_id);

    setup_margo_client();
    if (mid == MARGO_INSTANCE_NULL) {
      UMAP_ERROR("cannot initialize Margo client");
    }
  
    
    /* register a RPC */
    /* umap_rpc_in_t, umap_rpc_out_t are only significant on clients */
    /* uhg_umap_cb is only significant on the server */
    umap_request_rpc_id = MARGO_REGISTER(mid, "umap_request_rpc",
				       umap_request_rpc_in_t,
				       umap_request_rpc_out_t,
				       NULL);

    umap_release_rpc_id = MARGO_REGISTER(mid, "umap_release_rpc",
				    umap_release_rpc_in_t,
				    umap_release_rpc_out_t,
				    NULL);

    umap_read_rpc_id = MARGO_REGISTER(mid, "umap_read_rpc",
				       umap_read_rpc_in_t,
				       umap_read_rpc_out_t,
				       NULL);

    umap_write_rpc_id = MARGO_REGISTER(mid, "umap_write_rpc",
				    umap_write_rpc_in_t,
				    umap_write_rpc_out_t,
				    NULL);

  }
  
}


void client_fini(void)
{
  //rpc_clean_local_server_addr();

  /* shut down margo */
  //margo_finalize(mid);
  
  /* free memory allocated for context structure */
  //free(ctx);
}


int client_read_from_server(const char* id, void *buf_ptr, size_t nbytes, off_t off){

  RemoteResource &obj = resource_pool[id];
  int server_id=-1;
  off_t offset =off;
  
  /* Determine the server and offset on the server */
  if(obj.server_stride>0){
    server_id = offset/obj.server_stride;
    offset -= server_id * obj.server_stride;
  }else{
    std::vector<ServerMetadata>& metatable= obj.meta_table;
    
    for(auto it : metatable){
      if(it.offset<=off && off<(it.offset+it.size) ){

	//make sure no page is split over more than one server
	assert( (off+nbytes)<=(it.offset+it.size)  );
	server_id = it.server_id;
	offset = off-it.offset;
	break;
      }
    }
    assert( server_id>=0 );
  }

  //UMAP_LOG(Info, id<<" ["<< off <<", "<<nbytes<<"] from server "<< server_id << " [" << offset << ", " <<nbytes<<" ]");
  
  auto it = server_map.find(server_id);
  assert( it!=server_map.end());  
  hg_addr_t server_address = it->second;

  //UMAP_LOG(Info, id<<" [ 0x"<< buf_ptr << ", " << offset << ", " <<nbytes<<" ]");
  
  /* Forward the RPC. umap_client_fwdcompleted_cb will be called
   * when receiving the response from the server
   * After completion, user callback is placed into a
   * completion queue and can be triggered using HG_Trigger().
   */
  /* Create a RPC handle */
  hg_return_t ret;
  hg_handle_t handle;
  ret = margo_create(mid, server_address, umap_read_rpc_id, &handle);
  assert(ret == HG_SUCCESS);
    
  
  /* Create input structure
   * empty string attribute causes segfault 
   * because Mercury doesn't check string length before copy
   */
  umap_read_rpc_in_t in;
  in.id     = strdup(id);
  in.size   = nbytes;
  in.offset = offset;
  in.bulk_handle = HG_BULK_NULL;
  void **buf_ptrs    = (void **) &(buf_ptr);
  size_t *buf_sizes  = &(in.size);

  /* Create a bulk transfer handle in args */
  ret = margo_bulk_create(mid,
			  1, buf_ptrs, buf_sizes,
			  HG_BULK_READWRITE,
			  &(in.bulk_handle));
  assert(ret == HG_SUCCESS);
    
  
  /* Forward RPC requst to the server */
  ret = margo_forward(handle, &in);
  assert(ret == HG_SUCCESS);

    
  /* verify the response */
  umap_read_rpc_out_t out;
  ret = margo_get_output(handle, &out);
  assert(ret == HG_SUCCESS);
  assert( out.ret=RPC_RESPONSE_READ_DONE);
  margo_free_output(handle, &out);
 
  /* Free handle and bulk handles*/
  ret = margo_bulk_free(in.bulk_handle);
  assert(ret == HG_SUCCESS);
  ret = margo_destroy(handle);
  assert(ret == HG_SUCCESS);
  
  return ret;

}

int client_write_to_server(int server_id, void *buf_ptr, size_t nbytes, off_t offset){

  auto it = server_map.find(server_id);
  assert( it!=server_map.end());  
  hg_addr_t server_address = it->second;
  hg_return_t ret;
  
  /* Forward the RPC. umap_client_fwdcompleted_cb will be called
   * when receiving the response from the server
   * After completion, user callback is placed into a
   * completion queue and can be triggered using HG_Trigger().
   */
  /* Create a RPC handle */
  hg_handle_t handle;
  ret = margo_create(mid, server_address, umap_write_rpc_id, &handle);
  assert(ret == HG_SUCCESS);
    
  
  /* Create input structure
   * empty string attribute causes segfault 
   * because Mercury doesn't check string length before copy
   */
  umap_write_rpc_in_t in;
  in.size   = nbytes;
  in.offset = offset;
  in.bulk_handle = HG_BULK_NULL;
  void **buf_ptrs    = (void **) &(buf_ptr);
  size_t *buf_sizes  = &(in.size);

  /* Create a bulk transfer handle in args */
  ret = margo_bulk_create(mid,
			  1, buf_ptrs, buf_sizes,
			  HG_BULK_READ_ONLY,
			  &(in.bulk_handle));
  assert(ret == HG_SUCCESS);
    
  
  /* Forward RPC requst to the server */
  ret = margo_forward(handle, &in);
  assert(ret == HG_SUCCESS);

    
  /* verify the response */
  umap_write_rpc_out_t out;
  ret = margo_get_output(handle, &out);
  assert(ret == HG_SUCCESS);
  assert( out.ret=RPC_RESPONSE_WRITE_DONE);
  margo_free_output(handle, &out);
 
  /* Free handle and bulk handles*/
  ret = margo_bulk_free(in.bulk_handle);
  assert(ret == HG_SUCCESS);
  ret = margo_destroy(handle);
  assert(ret == HG_SUCCESS);
  
  return ret;

}
