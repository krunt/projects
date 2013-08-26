#define _XOPEN_SOURCE

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <event.h>
#include "master.h"
#include "state.h"
#include "shared_state.h"
#include "connection.h"
#include "locks.h"
#include "packet.h"
#include "logger.h"
#include "shutdown.h"

int main() {
    struct event_base *base;
    struct state state;

    logger_init("./tmp/log");

    base = event_base_new();
    if (!base) {
        return -1;
    }

    if (init_state(&state, base) == -1) {
        return -1;
    }

    int i_am_alone = 1;

    sem_lock();
    shared_segment_t segment = read_memory_segment();
    if ((shared_segment_t)-1 == segment) {
        sem_unlock();
        return -2;
    }

    state.segment = segment;

    /*
    drop_memory_segment(&state);
    sem_unlock();
    return -3;
    */

    for (int i = 0; i < MAX_PIDS; ++i) {
        state.ids[i].pid = segment->pids[i];

        if (segment->pids[i] != INVALID && segment->pids[i] != state.myid.pid) {
            i_am_alone = 0;
        }
    }
    if (init_connections(&state) == -1 
            || add_shared_pid(&state, state.myid.pid) == -1) {
        sem_unlock();
        return -2;
    }
    if (i_am_alone) {
        segment->master_pid = state.myid.pid;
    }
    state.master_pid = segment->master_pid;
    sem_unlock();

    int index = add_local_pid(&state, state.myid.pid);
    if (index != -1) {
        state.ids[index].fd = init_connection(&state, state.myid.pid);
    }

    mlog(LOG_INFO, "%d master_pid=%d, pid=%d, fd=%d", i_am_alone, 
            state.master_pid, state.myid.pid, state.myid.fd);

    mlog(LOG_INFO, "state.ids=");
    for (int i = 0; i < MAX_PIDS; i+=8) {
        for (int j = 0; j < 8; ++j) {
            mlog(LOG_INFO, "[%d]=%d,%d", i+j, state.ids[i+j].pid, state.ids[i+j].fd);
        }
    }
    mlog(LOG_INFO, "shared segment");
    for (int i = 0; i < MAX_PIDS; i+=8) {
        for (int j = 0; j < 8; ++j) {
            mlog(LOG_INFO, "[%d]=%d", i+j, state.segment->pids[i+j]);
        }
    }

    void *packet = build_packet(PEER_ADDED, state.myid.pid);
    send_packet(&state, packet, sizeof(struct packet), 0);
    free(packet);

    if (i_am_alone) {
        begin_mastering(&state);
    }

    initialize_shutdown(&state);

    event_assign(&state.myid.event, base, state.myid.fd, 
                EV_READ | EV_PERSIST, &packet_handler, (void*)&state);
    event_add(&state.myid.event, NULL);

    event_base_dispatch(base);
    event_base_free(base);

    return 0;
}
