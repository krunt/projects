#include "master.h"
#include "packet.h"
#include <event.h>
#include "config.h"
#include <stdlib.h>
#include <event2/util.h>
#include <assert.h>
#include "connection.h"
#include "logger.h"

void
sending_count(int fd, short event, void *arg) {
    state_t state = (state_t)arg;
    void *packet = build_packet(COUNTING, ++state->master_count);
    send_packet(state, packet, sizeof(struct packet), 0);
    free(packet);
}

void
begin_mastering(state_t state) {
    mlog(LOG_INFO, "begin_mastering");

    state->master_count = 0;
    event_assign(&state->master_event, state->base,
            -1, EV_PERSIST, &sending_count, (void*)state);

    struct timeval tv;
    tv.tv_usec = 0;
    tv.tv_sec = PERIOD_INTERVAL;
    event_add(&state->master_event, &tv);
}

int
determine_master_pid(state_t state) {
    int min_pid = (1<<30);
    for (int i = 0; i < MAX_PIDS; ++i) {
        int pid = state->ids[i].pid;
        if (pid != INVALID && pid < min_pid) {
            min_pid = pid;
        }
    }
    assert(min_pid >= 0);
    return min_pid;
}
