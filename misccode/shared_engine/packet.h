#ifndef PACKET_SE__
#define PACKET_SE__

#include "state.h"

// packet types
enum {
    PEER_ADDED,
    PEER_DROPPED,
    COUNTING,
};

// type, packet-length, payload
struct packet {
    int type;
    int length;
    int term;
};
typedef struct packet *packet_t;

void *
build_packet(int type, int term);

void
packet_handler(int fd, short event, void *arg);

void
send_packet(state_t state, void *packet, int length, int to_me);

#endif
