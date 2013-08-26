#define _XOPEN_SOURCE

#include "shared_state.h"
#include "shutdown.h"
#include "state.h"
#include "locks.h"
#include "packet.h"
#include <event.h>
#include <signal.h>
#include <stdlib.h>
#include "connection.h"
#include "packet.h"

void
signal_callback(int fd, short event, void *arg) {
    state_t state = (state_t)arg;

    event_del(&state->myid.event);

    // if master
    if (state->master_pid == state->myid.pid) {
        event_del(&state->master_event);
    }

    sem_lock();
    del_shared_pid(state, state->myid.pid);
    sem_unlock();

    void *packet = build_packet(PEER_DROPPED, state->myid.pid);
    send_packet(state, packet, sizeof(struct packet), 0);
    free(packet);

    del_local_pid(state, state->myid.pid);

    int count = 0;
    for (int i = 0; i < MAX_PIDS; ++i) {
        if (state->ids[i].pid != INVALID) {
            count++;
        }
    }

    // dropping shm segment
    if (!count) {
        drop_memory_segment(state);
    }

    // loop shutdown
}

int
initialize_shutdown(state_t state) {
    event_assign(&state->shutdown_event, state->base,
            SIGINT, EV_SIGNAL, &signal_callback, (void*)state);
    event_add(&state->shutdown_event, NULL);
}
