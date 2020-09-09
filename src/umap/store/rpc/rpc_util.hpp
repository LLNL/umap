#ifndef _RPC_UTIL_H
#define _RPC_UTIL_H

#include <vector>
#include <map>
#include <string>
#include <margo.h>
#include "mercury_macros.h"
#include "mercury_proc_string.h"


#define MAX_ADDR_LENGTH 64
#define LOCAL_RPC_ADDR_FILE "serverfile"
#define RPC_RESPONSE_READ_DONE 1234
#define	RPC_RESPONSE_WRITE_DONE 4321
#define	RPC_RESPONSE_REQ_UNAVAIL 7777
#define RPC_RESPONSE_REQ_WRONG_SIZE 9999
#define RPC_RESPONSE_REQ_SIZE 5555
#define RPC_RESPONSE_RELEASE 6666
#define	RPC_RESPONSE_GENERAL_ERROR 1111

typedef struct server_metadata
{
  size_t offset;
  size_t size;
  size_t server_id;
  server_metadata(size_t o, size_t s, size_t id)
    :offset(o), size(s), server_id(id){}
}ServerMetadata;

typedef struct remote_resource
{
  size_t rsize;
  size_t num_servers;
  size_t server_stride; //for evenly distributed resource
  std::vector<ServerMetadata> meta_table;//for irregular distrbuted resource
  remote_resource(){}
  remote_resource(size_t s, size_t n)
    :rsize(s), num_servers(n) {}
} RemoteResource;

typedef std::map<std::string, RemoteResource> ClientResourcePool;

typedef struct local_resource
{
  void *ptr;
  size_t rsize;
  size_t num_clients;
  local_resource(){}
  local_resource(void* p, size_t s, size_t n=0)
    :ptr(p), rsize(s), num_clients(n) {}
} LocalResource;

typedef std::map<std::string, LocalResource> ServerResourcePool;


#ifdef __cplusplus
extern "C" {
#endif

/* UMap RPC input structure */
MERCURY_GEN_PROC(umap_read_rpc_in_t,
                 ((hg_const_string_t)(id))\
                 ((hg_size_t)(size))\
                 ((hg_size_t)(offset))\
                 ((hg_bulk_t)(bulk_handle)))

/* UMap RPC output structure */
MERCURY_GEN_PROC(umap_read_rpc_out_t,
		 ((int32_t)(ret)))

/* UMap RPC input structure */
MERCURY_GEN_PROC(umap_write_rpc_in_t,
                 ((hg_const_string_t)(id))\
                 ((hg_size_t)(size))\
                 ((hg_size_t)(offset))\
                 ((hg_bulk_t)(bulk_handle)))

/* UMap RPC output structure */
MERCURY_GEN_PROC(umap_write_rpc_out_t,
		 ((int32_t)(ret)))

/* UMap RPC input structure */
MERCURY_GEN_PROC(umap_request_rpc_in_t,
		 ((hg_const_string_t)(id))\
                 ((hg_size_t)(size)))

/* UMap RPC output structure */
MERCURY_GEN_PROC(umap_request_rpc_out_t,
                 ((hg_size_t)(size))\
		 ((int32_t)(ret)))

/* UMap RPC input structure */
MERCURY_GEN_PROC(umap_release_rpc_in_t,
		 ((hg_const_string_t)(id)))

/* UMap RPC output structure */
MERCURY_GEN_PROC(umap_release_rpc_out_t,
		 ((int32_t)(ret)))

#ifdef __cplusplus
} // extern "C"
#endif
  
#endif
