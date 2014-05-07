#include "connection.h"

#include <string.h>
#include <unistd.h>

int connection_open(connection_t *conn, const char *host, int port,
        char *error_message, int error_message_size)
{
    fdsocket_t fd, flags;
    struct hostent *ht;

    memset(conn, 0, sizeof(*conn));
    conn->error_message = error_message;
    conn->error_message_size = error_message_size;

    if ((*myos()->socket_create)(&fd, PF_INET, SOCK_STREAM, 0)) {
        goto error;
    }

    if (!(ht = (*myos()->socket_gethostbyname)(host))) {
        goto error;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = PF_INET;
    addr.sin_port = (*myos()->socket_htons)(port);
    memcpy(&addr.sin_addr, ht->h_addr, sizeof(struct in_addr));

    const int val = 1;
    if ((*myos()->socket_setsockopt)(fd, 
        SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) < 0) 
    { goto error; }

    if ((*myos()->socket_connect)(fd, 
        (const struct sockaddr *)&addr, sizeof(addr)))
    { goto error; }

    (*myos()->socket_set_blocking)(fd, 0);

    conn->fd = fd;
    return 0;

error:
    (*myos()->get_last_socket_error_message)(
        conn->error_message, conn->error_message_size);
    return 1;
}

int connection_read(connection_t *conn, char *buf, int size) {
    int err;
    char *original_buf = buf;
    int left = size;
    while (left > 0) {
        int read_bytes = (*myos()->socket_read)(conn->fd, buf, left);
        if (read_bytes <= 0) {
            err = read_bytes;
            if (read_bytes < 0 && err == MYOS_EINTR)
                continue;
            if (read_bytes < 0 && err == MYOS_EAGAIN)
                break;
            if (!read_bytes)
                return CONNECTION_READ_EOF;
            (*myos()->get_last_socket_error_message)(
                conn->error_message, conn->error_message_size);
            return CONNECTION_READ_ERROR;
        }
        buf += read_bytes;
        left -= read_bytes;
    }
    return buf - original_buf;
}

int connection_write(connection_t *conn, char *buf, int size) {
    int err, left = size;
    while (left > 0) {
        int written_bytes = (*myos()->socket_write)(conn->fd, buf, left);
        if (written_bytes <= 0) {
            err = written_bytes;
            if (written_bytes < 0 && (err == MYOS_EINTR || err == MYOS_EAGAIN))
                continue;
            (*myos()->get_last_socket_error_message)(
                conn->error_message, conn->error_message_size);
            return 1;
        }
        buf += written_bytes;
        left -= written_bytes;
    }
    return 0;
}

/* timeout in ms, -1 - infinitely */
int connection_poll(connection_t *conn, int for_read, int timeout) {
    int rc;
    fd_set fdset;
    struct timeval timeout_struct;

    memset(&timeout_struct, 0, sizeof(timeout_struct));
    if (timeout != -1) { timeout_struct.tv_usec = 1000 * timeout; }

again:
    FD_ZERO(&fdset);
    FD_SET(conn->fd, &fdset);
    if ((rc = (*myos()->socket_select)(conn->fd + 1, for_read ? &fdset : NULL,
        for_read ? NULL: &fdset, NULL, timeout == -1 ? NULL : &timeout_struct)) 
            < 0)
    {
        if (rc == MYOS_EINTR)
            goto again;

        (*myos()->get_last_socket_error_message)(
                conn->error_message, conn->error_message_size);
        return 1; 
    }

    return 0;
}

int connection_close(connection_t *conn) {
    if ((*myos()->socket_close)(conn->fd)) {
        (*myos()->get_last_socket_error_message)(
                conn->error_message, conn->error_message_size);
        return 1;
    }
    return 0;
}
