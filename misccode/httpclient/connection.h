#ifndef CONNECTION_DEF_
#define CONNECTION_DEF_

#include "error.h"

#include "myos.h"

#define CONNECTION_READ_ERROR -2
#define CONNECTION_READ_NOT_READY -3
#define CONNECTION_READ_EOF -4

typedef struct connection_s {
    fdsocket_t fd;

    char *error_message;
    int error_message_size;

} connection_t;

int connection_open(connection_t *conn, const char *host, int port, 
        char *error_message, int error_message_size);
int connection_read(connection_t *conn, char *buf, int size);
int connection_write(connection_t *conn, char *buf, int size);
int connection_poll(connection_t *conn, int for_read, int timeout);
int connection_close(connection_t *conn);

#endif /* CONNECTION_DEF_ */
