#include "state.h"
#include "connection.h"
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int
init_state(state_t state, struct event_base *base) {
    memset(state, 0, sizeof(*state)); 
    state->base = base;
    state->myid.pid = getpid();
    state->myid.fd = init_socket(state);
    for (int i = 0; i < MAX_PIDS; ++i)
        state->ids[i].pid = INVALID;
    return state->myid.fd;
}

int
add_local_pid(state_t state, int pid) {
    for (int i = 0; i < MAX_PIDS; ++i) {
        if (state->ids[i].pid == INVALID) {
            state->ids[i].pid = pid;
            return i;
        }
    }
    // no space
    return -1;
}

int
del_local_pid(state_t state, int pid) {
    for (int i = 0; i < MAX_PIDS; ++i) {
        if (state->ids[i].pid == pid) {
            state->ids[i].pid = INVALID;
            return 0;
        }
    }
    return -1;
}
