#ifndef __PACKET__
#define __PACKET__

#include <event.h>

enum { PORT = 9999, MAXLOG = 127, };

enum {
    PRE_READING = 0,
    READING = 1,
    PRE_WRITING = 2,
    WRITING = 3,
    ENDING = 4,
};

enum {
    READING_HEADER = 0,
    READING_PAYLOAD = 1,
};

enum {
    WRITING_HEADER = 0,
    WRITING_PAYLOAD = 1,
};

enum { MAX_PACKET_SIZE = 1024 };

struct packet_header {
    int packet_type;
    int payload_length;
};

typedef struct packet_header *packet_header_t;

struct packet_state {
    int state;
    int processed_header;
    int total_header;
    char header[sizeof(struct packet_header)];
    int processed_payload;
    int total_payload;
    char buffer[MAX_PACKET_SIZE];
};

typedef struct event *event_t;
typedef struct packet_state *packet_state_t;

struct connector;

struct acceptor {
    int fd;
    struct event event;
    struct connector *head;
};

struct connector {
    int fd;
    int state;
    struct event event;
    struct packet_state rstate;
    struct packet_state wstate;
    struct connector *next;
};

typedef struct acceptor *acceptor_t;
typedef struct connector *connector_t;

#endif
