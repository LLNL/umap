#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#define UMAP_SERVER_PATH "/tmp/umap-server"
void init_umap_client(const char *sock_path);
void close_umap_client();

void* client_umap(
    const char *filename
  , int prot
  , int flags
  , void *addr
);

int client_uunmap(
    const char *filename
);
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
