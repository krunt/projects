#include <myclib/all.h>
#include <myos.h>

#include "url.h"

typedef struct my_tcp_state_s {
    fdsocket_t fd;
} my_tcp_state_t;

my_tcp_state_t *my_tcp_open(const char *host, int port) {
    struct sockaddr_in addr;
    struct hostent *ht;
    my_tcp_state_t *res = malloc(sizeof(my_tcp_state_t));
    if ((*myos()->socket_create)(&res->fd, AF_INET, SOCK_STREAM, 0))
        return NULL;

    if (!(ht = (*myos()->socket_gethostbyname)(host)))
        return NULL;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = PF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, ht->h_addr, sizeof(struct in_addr));

    if ((*myos()->socket_connect)(res->fd, (struct sockaddr *)&addr, 
                sizeof(addr))) 
    { return NULL; }
    return res;
}

int my_tcp_read(my_tcp_state_t *st, u8 *buf, int size) {
    return (*myos()->socket_read)(st->fd, buf, size);
}

int my_tcp_write(my_tcp_state_t *st, u8 *buf, int size) {
    return (*myos()->socket_write)(st->fd, buf, size);
}

int my_tcp_close(my_tcp_state_t *st) {
    return (*myos()->socket_close)(st->fd);
}

my_url_proto_t my_tcp_proto = {
    .name = "tcp",
    .open = (void*(*)(const char*,int))&my_tcp_open,
    .read = (int(*)(void*,u8*,int))&my_tcp_read,
    .write = (int(*)(void*,u8*,int))&my_tcp_write,
    .close = (int(*)(void*))&my_tcp_close,
};

