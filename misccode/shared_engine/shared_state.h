#ifndef SHARED_STATE_SE__
#define SHARED_STATE_SE__

#define _XOPEN_SOURCE

#include "config.h"
#include <sys/types.h>
#include <sys/ipc.h>

struct shared_segment {
    int master_pid;
    int pids[MAX_PIDS];
};

typedef struct shared_segment *shared_segment_t;

int
create_memory_segment(key_t keyid, int size);


shared_segment_t
read_memory_segment();

struct state;

int
add_shared_pid(struct state *state, int pid);

int
del_shared_pid(struct state *state, int pid);

int
drop_memory_segment(struct state *state);

#endif
