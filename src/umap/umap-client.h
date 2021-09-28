//////////////////////////////////////////////////////////////////////////////
//// Copyright 2017-2021 Lawrence Livermore National Security, LLC and other
//// UMAP Project Developers. See the top-level LICENSE file for details.
////
//// SPDX-License-Identifier: LGPL-2.1-only
////////////////////////////////////////////////////////////////////////////////


#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#define UMAP_SERVER_PATH "/tmp/umap-server"

/*!
 * @function    init_umap_client
 * @abstract    initializes connection with an already running mpumap service
 * @param       sock_path 	Path to Unix Domain socket hosted by target 
 * 				mpumap service. When a NULL value is passed,
 * 				a default UMAP_SERVER_PATH is used  
 * @result      Exits if it fails to connect to the service. 
*/  
void init_umap_client(const char *sock_path);

/*!
 * @function    close_umap_client
 * @abstract    Closes an already established mpumap service connection
 * @result      Exits if no service is found to be connected with 
*/  
void close_umap_client();


/*!
 * @function    client_umap
 * @abstract    Functionally equivalent to mmaping a file. Should be called only 
 * 		after connection with target umap-service has been established using init_umap_client
 * @param       filename 	Full path to the file to be memory mapped
 * @param	prot 		At present it only accepts PROT_READ as we serve read-only buffers
 * 				This parameter is to catch instances where users intend to use the buffer
 * 				other than read-only purposes.
 * @param	flags		At present it only accepts MAP_SHARED as these buffers are supposed to
 * 				be shared between multiple processes, which include the mpumap service.
 * 				This is to catch cases where user intends to use it otherwise.
 * @param 	addr		non-NULL value is intended for fixed address. Floating address otherwise.
 * @result      Return NULL on failure. Else return the userspace mapped address 
*/  
void* client_umap(
    const char *filename
  , int prot
  , int flags
  , void *addr
);


/*!
 * @function    init_umap_client
 * @abstract    Removes the mapping from the client process' address space. 
 * @param       filename 	Path to file that has previously been mapped 
 * 				by client_umap call.
 * @result      Return -1 on error, 0 On Success
*/  
int client_uunmap(
    const char *filename
);

/* 
 * The following mpumap client API calls provide visibility to mpumap 
 * service's settings to the client. This allows clients to use these
 * values as they deem necessary. These calls need to be called after 
 * establishing connection with a mpumap service through init_umap_client 
 * API call. 
 */
long umapcfg_get_system_page_size( void );
uint64_t umapcfg_get_max_pages_in_buffer( void );
uint64_t umapcfg_get_umap_page_size( void );
uint64_t umapcfg_get_num_fillers( void );
uint64_t umapcfg_get_num_evictors( void );
int umapcfg_get_evict_low_water_threshold( void );
int umapcfg_get_evict_high_water_threshold( void );
uint64_t umapcfg_get_max_fault_events( void );

#ifdef __cplusplus
}
#endif
