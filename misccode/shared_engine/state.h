#ifndef STATE_SE__
#define STATE_SE__

#include "config.h"
#include <event.h>

struct process_ident {
    int pid;
    int fd;
    struct event event;
};
typedef struct process_ident *process_t;

struct shared_segment;
struct state {
    struct event_base *base;
    struct process_ident myid;
    struct process_ident ids[MAX_PIDS];

    struct event master_event;
    int master_count;
    int master_pid;

    struct event shutdown_event;

    struct shared_segment *segment;
};

typedef struct state *state_t;

int
init_state(state_t state, struct event_base *base);

int
add_local_pid(state_t state, int pid);

int
del_local_pid(state_t state, int pid);

#endif
