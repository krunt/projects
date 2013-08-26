#define _XOPEN_SOURCE

#include "packet.h"
#include "master.h"
#include <event.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include "connection.h"

void *
build_packet(int type, int term) {
    void *packet = malloc(sizeof(struct packet)); 
    packet_t p = (packet_t)packet;
    p->type = type;
    p->length = sizeof(term);
    p->term = term;
    return packet;
}

void
packet_handler(int fd, short event, void *arg) {
    state_t state = (state_t)arg;
    assert(state->myid.fd == fd);
    struct packet packet;
    read(fd, &packet, sizeof(struct packet));

    switch (packet.type) {
      case PEER_ADDED: {
        int pid = packet.term;
        if (pid != state->myid.pid) {
            add_local_pid(state, pid);
            init_connection(state, pid);
        }
        break;
      }
      case PEER_DROPPED: {
        int pid = packet.term;
        if (pid != state->myid.pid) {
            close_connection(state, pid);
            del_local_pid(state, pid);
        }
        if (pid == state->master_pid) {
            state->master_pid = determine_master_pid(state);
            // we are a master
            if (state->master_pid == state->myid.pid) {
                begin_mastering(state);
            }
        }
        break;
      }
      case COUNTING: {
        int count = packet.term;
        printf("%d = %d\n", getpid(), count);
        break;
      }
    };
}

void
send_packet(state_t state, void *packet, int length, int to_me) {
    for (int i = 0; i < MAX_PIDS; ++i) {
        if (state->ids[i].pid == INVALID)
            continue;
        if (state->ids[i].pid == state->myid.pid && !to_me)
            continue;
        write(state->ids[i].fd, packet, length);
    }
}
