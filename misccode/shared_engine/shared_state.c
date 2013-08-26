#define _XOPEN_SOURCE

#include "state.h"
#include "shared_state.h"
#include <errno.h>
#include <sys/shm.h>
#include "config.h"

int
create_memory_segment(key_t keyid, int size) {
    int id = shmget(keyid, size, IPC_CREAT | 0777);
    if (id == -1)
        return -1;

    void *p = shmat(id, 0, 0);
    if (p == (void *)-1)
        return -1;

    shared_segment_t segment = (shared_segment_t)p;
    segment->master_pid = INVALID;
    for (int i = 0; i < MAX_PIDS; ++i)
        segment->pids[i] = INVALID;
    return 0; //shmdt(p);
}

shared_segment_t
read_memory_segment() {
    key_t keyid = ftok(MEM_IDENT_PATH, 128);
    if (keyid == -1) 
        return (shared_segment_t)-1;

    int id = shmget(keyid, sizeof(struct shared_segment), 0);
    if (id == -1) {
        if (errno != ENOENT 
            || ((id = create_memory_segment(keyid, 
                        sizeof(struct shared_segment))) == -1)) 
        { return (shared_segment_t)-1; }
    }
    
    // get for read/write
    void *p = shmat(id, 0, 0);
    if (p == (void *)-1)
        return (shared_segment_t)-1;

    return (shared_segment_t)p;
}

int
drop_memory_segment(state_t state) {
    if (shmdt(state->segment) == -1)
        return -1;

    key_t keyid = ftok(MEM_IDENT_PATH, 128);
    if (keyid == -1) 
        return -1;
    
    int id = shmget(keyid, sizeof(struct shared_segment), 0);
    return shmctl(id, IPC_RMID, 0);
}

int
add_shared_pid(state_t state, int pid) {
    for (int i = 0; i < MAX_PIDS; ++i) {
        if (state->segment->pids[i] == INVALID) {
            state->segment->pids[i] = pid;
            return 0;
        }
    }
    // no space
    return -1;
}

int
del_shared_pid(state_t state, int pid) {
    for (int i = 0; i < MAX_PIDS; ++i) {
        if (state->segment->pids[i] == pid) {
            state->segment->pids[i] = INVALID;
            return 0;
        }
    }
    return -1;
}
