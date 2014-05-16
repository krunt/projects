#ifndef URL_DEF_
#define URL_DEF_

#include <myclib/all.h>

typedef struct my_url_proto_s {
    const char *name;

    void *(*open)(const char *host, int port);
    int (*read)(void *opaque, u8 *buf, int size);
    int (*write)(void *opaque, u8 *buf, int size);
    int (*close)(void *opaque);

    void *arg;
} my_url_proto_t;

my_url_proto_t *my_url_open(const char *proto, const char *host, int port);
int my_url_read(my_url_proto_t *proto, u8 *buf, int sz);
int my_url_write(my_url_proto_t *proto, u8 *buf, int sz);
int my_url_close(my_url_proto_t *proto);

#endif /* URL_DEF_ */
