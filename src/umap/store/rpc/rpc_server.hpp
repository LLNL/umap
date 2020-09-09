#ifndef _RPC_SERVER_H
#define _RPC_SERVER_H

void server_init(void);
void server_fini(void);
void server_start(size_t _num_clients);

int server_add_resource(const char* id, void* ptr, size_t rsize, size_t num_clients=0);
int server_delete_resource(const char* id);

#endif
