#ifndef CONNECTION_SE__
#define CONNECTION_SE__

#include "state.h"

void 
pid2name(int pid, char *buf);

int
init_socket(state_t state);

int
init_connection(state_t state, int pid);

int 
init_connections(state_t state);

int
close_connection(state_t state, int pid);

#endif
