#include "packet.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include "logger.h"

void set_nonblockable(int fd) {
    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | O_NONBLOCK);
}

int create_listener_socket(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        return fd;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        return -1;
    }

    if (listen(fd, MAXLOG) == -1) {
        return -1;
    }

    set_nonblockable(fd);

    const int one = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&one, sizeof(one)) == -1) {
        return -1;
    }
    
    return fd;
}

int
get_peer_address(int fd) {
    struct sockaddr_in saddr;
    int addrlen = sizeof(saddr);
    if (getpeername(fd, (struct sockaddr *)&saddr, &addrlen) != 0) {
        return 0;
    }
    return (int)saddr.sin_addr.s_addr;
}

static void
connector_handler(int fd, short event, void *arg);

static void
regenerate_connector_event(connector_t connector) {
    event_add(&connector->event, NULL);
}

static void
read_packet(connector_t connector) {
    packet_state_t rstate = &connector->rstate;

    int to_read;
    char *buffer;
    switch (rstate->state) {
        case READING_HEADER:
            to_read = rstate->total_header - rstate->processed_header;
            buffer = rstate->header;
        break;
        case READING_PAYLOAD:
            to_read = rstate->total_payload - rstate->processed_payload;
            buffer = rstate->buffer;
        break;
        default:
            assert(0);
    };

    int was_read = read(connector->fd, buffer, to_read);
    if (was_read <= 0 && errno != EWOULDBLOCK) {
        connector->state = ENDING;
        return;
    }

    switch (rstate->state) {
        case READING_HEADER:
            rstate->processed_header += was_read;
            if (rstate->processed_header == rstate->total_header) {
                rstate->state = READING_PAYLOAD;
                rstate->total_payload 
                    = ntohl(((packet_header_t)buffer)->payload_length);
                mlog(LOG_INFO, "read packet header from %X", 
                        get_peer_address(connector->fd));
            }
        break;
        case READING_PAYLOAD:
            rstate->processed_payload += was_read;
            if (rstate->processed_payload == rstate->total_payload) {
                connector->state = PRE_WRITING;
                mlog(LOG_INFO, "read whole packet from %X", 
                        get_peer_address(connector->fd));
            }
        break;
    };
}

static void
write_packet(connector_t connector) {
    packet_state_t wstate = &connector->wstate;

    int to_write;
    char *buffer;
    switch (wstate->state) {
        case WRITING_HEADER:
            to_write = wstate->total_header - wstate->processed_header;
            buffer = wstate->header;
        break;
        case WRITING_PAYLOAD:
            to_write = wstate->total_payload - wstate->processed_payload;
            buffer = wstate->buffer;
        break;
        default:
            assert(0);
    };

    int was_written = write(connector->fd, buffer, to_write);
    if (was_written <= 0 && errno != EWOULDBLOCK) {
        connector->state = ENDING;
        return;
    }

    switch (wstate->state) {
        case WRITING_HEADER:
            wstate->processed_header += was_written;
            if (wstate->processed_header == wstate->total_header) {
                wstate->state = WRITING_PAYLOAD;
                mlog(LOG_INFO, "wrote packet header to %X", 
                    get_peer_address(connector->fd));
            }
        break;
        case WRITING_PAYLOAD:
            wstate->processed_payload += was_written;
            if (wstate->processed_payload == wstate->total_payload) {
                connector->state = ENDING;
                mlog(LOG_INFO, "wrote whole packet to %X", 
                    get_peer_address(connector->fd));
            }
        break;
    };
}

static void
connector_handler(int fd, short event, void *arg) {
    connector_t connector = (connector_t)arg;
again:
    switch (connector->state) {
        case PRE_READING: {
            packet_state_t rstate = &connector->rstate;
            memset((void *)rstate, 0, sizeof(*rstate));
            rstate->state = READING_HEADER;
            rstate->total_header = sizeof(struct packet_header);
            connector->state = READING;
        }

        case READING:
            if (!(event & EV_READ)) { 
                break; 
            }
            read_packet(connector);
        break;

        case PRE_WRITING: {
            packet_state_t wstate = &connector->wstate;
            memset((void *)wstate, 0, sizeof(*wstate));
            wstate->state = WRITING_HEADER;
            wstate->total_header = sizeof(struct packet_header);
            memcpy(wstate->header, wstate->header, sizeof(sizeof(struct packet_header)));
            memcpy(wstate->buffer, wstate->buffer, wstate->total_payload);
            connector->state = WRITING;
        }

        case WRITING:
            if (!(event & EV_WRITE)) { 
                break; 
            }
            write_packet(connector);
        break;
            
        case ENDING:
            mlog(LOG_INFO, "terminating connection with %X", 
                get_peer_address(connector->fd));
            return;

        default:
            assert(0);
    };
    if (connector->state == ENDING)
        goto again;
    regenerate_connector_event(connector);
}

static void 
acceptor_handler(int fd, short event, void *arg) {
    acceptor_t acceptor = (acceptor_t)arg;
    int peer_fd = accept(acceptor->fd, NULL, NULL);
    if (peer_fd < 0) {
        return;
    }
    mlog(LOG_INFO, "got connection from %X", get_peer_address(peer_fd));
    connector_t connector = (connector_t)malloc(sizeof(struct connector));
    memset((void *)connector, 0, sizeof(struct connector));
    set_nonblockable(peer_fd);
    connector->fd = peer_fd;
    connector->state = PRE_READING;
    if (acceptor->head) {
        connector->next = acceptor->head;
        acceptor->head = connector;
    } else {
        acceptor->head = connector;
    }
    event_set(&connector->event, connector->fd, 
        EV_READ | EV_WRITE, connector_handler, (void *)connector);
    regenerate_connector_event(connector);
}

int
initialize_acceptor(acceptor_t acceptor) {
    acceptor->fd = create_listener_socket(PORT);
    if (acceptor->fd == -1) {
        return -1;
    }
    event_set(&acceptor->event, acceptor->fd, 
                EV_READ | EV_PERSIST, acceptor_handler, (void *)acceptor);
    event_add(&acceptor->event, NULL);
    return acceptor->fd;
}

void
free_acceptor(acceptor_t acceptor) {
    connector_t connector = acceptor->head;
    while (connector) {
        connector_t tmp = connector;
        connector = connector->next;
        free((void *)tmp);
    }
}

int main() {
    event_init();
    logger_init(".server_log");
    struct acceptor acceptor;
    if (initialize_acceptor(&acceptor) == -1) {
        return -1;
    }
    event_dispatch();
    free_acceptor(&acceptor);
    return 0;
}
