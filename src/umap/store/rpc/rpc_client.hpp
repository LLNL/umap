#ifndef _RPC_CLIENT_H
#define _RPC_CLIENT_H


void client_init(void);
void client_fini(void);

int  client_read_from_server(const char*id, void *buf_ptr, size_t nbytes, off_t offset);
int  client_write_to_server(int server_id, void *buf_ptr, size_t nbytes, off_t offset);

bool client_check_resource(const char*id);
void client_request_resource(const char* id, size_t rsize);
int  client_release_resource(const char* id);
bool client_add_resource(const char*id, size_t rsize);
size_t client_get_resource_size(const char*id);

#endif
