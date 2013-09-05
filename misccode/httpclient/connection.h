#include "error.h"

typedef struct connection_s {
    int fd;

    char *error_message;
    int error_message_size;

} connection_t;

int connection_open(connection_t *conn, const char *host, int port, 
        char *error_message, int error_message_size);
int connection_read(connection_t *conn, char *buf, int size);
int connection_write(connection_t *conn, char *buf, int size);
int connection_poll(connection_t *conn, int for_read);
int connection_close(connection_t *conn);

