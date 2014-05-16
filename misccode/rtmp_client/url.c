#include "url.h"
#include "tcp.h"

#include <myos.h>

#include <string.h>
#include <errno.h>

my_url_proto_t *protos[] = {
    &my_tcp_proto,
    NULL,
};

my_url_proto_t *my_url_open(const char *proto, const char *host, int port) {
    my_url_proto_t **p = protos, *res;
    for (; *p; (*p)++) {
        if (!strcmp((*p)->name, proto))
            goto found;
    }
    return NULL;
found:
    res = malloc(sizeof(my_url_proto_t));
    res->open = (void*(*)(const char*,int))(*p)->open;
    res->read = (int(*)(void*,u8*,int))(*p)->read;
    res->write = (int(*)(void*,u8*,int))(*p)->write;
    res->close = (int(*)(void*))(*p)->close;
    if (!(res->arg = (*res->open)(host, port)))
        return NULL;
    return res;
}

int my_url_read(my_url_proto_t *proto, u8 *buf, int sz) {
    int rc;
    u8 *p = buf;
    while (sz) {
    again:
        rc = (*proto->read)(proto->arg, p, sz);
        if (rc <= 0) {
            if (errno == MYOS_EINTR || errno == MYOS_EAGAIN)
                goto again;
            my_log_error(rc == 0 ? "premature read close" : "my_url_read error");
            return 1;
        }
        p += rc;
        sz -= rc;
    }
    return 0;
}

int my_url_write(my_url_proto_t *proto, u8 *buf, int sz) {
    int rc;
    u8 *p = buf;
    while (sz) {
    again:
        rc = (*proto->write)(proto->arg, p, sz);
        if (rc < 0) {
            if (errno == MYOS_EINTR || errno == MYOS_EAGAIN)
                goto again;
            my_log_error("my_url_write error");
            return 1;
        }
        p += rc;
        sz -= rc;
    }
    return 0;
}

int my_url_close(my_url_proto_t *proto) {
    proto->close(proto->arg);
    proto->arg = NULL;
    free(proto);
    return 0;
}
