#include "connection.h"

int connection_open(connection_t *conn, const char *host, int port,
        char *error_message = NULL, int error_message_size = 0) 
{
    int fd, flags;
    struct hostent *ht;

    memset(conn, 0, sizeof(*conn));
    conn->error_message = error_message;
    conn->error_message_size = error_message_size;

    if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        goto error;
    }

    /* set nonblocking */
    if ((flags = fcntl(fd, F_GETFD)) < 0
        || fcntl(fd, F_SETFD, flags | O_NONBLOCK) < 0)
    { goto error; }

    if (!(ht = gethostbyname(host))) {
        goto error;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = PF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, ht->h_addr, sizeof(struct in_addr));

    const int val = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) < 0) {
        goto error;
    }

    if (connect(fd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
        goto error;
    }

    conn->fd = fd;
    return 0;

error:
    strerror_r(errno, conn->error_message, sizeof(conn->error_message));
    return 1;
}

int connection_read(connection_t *conn, char *buf, int size) {
    int left = size;
    while (left > 0) {
        int read_bytes = read(fd, buf, left);
        if (read_bytes <= 0) {
            if (errno == EINTR)
                continue;
            strerror_r(errno, conn->error_message, sizeof(conn->error_message));
            return 1;
        }
        buf += read_bytes;
        left -= read_bytes;
    }
    return 0;
}

int connection_write(connection_t *conn, char *buf, int size) {
    int left = size;
    while (left > 0) {
        int written_bytes = write(fd, buf, left);
        if (written_bytes <= 0) {
            if (errno == EINTR)
                continue;
            strerror_r(errno, conn->error_message, sizeof(conn->error_message));
            return 1;
        }
        buf += written_bytes;
        size -= written_bytes;
    }
    return 0;
}

/* timeout in ms, -1 - infinitely */
int connection_poll(connection_t *conn, int for_read, int timeout = -1) {
    fd_set fdset;
    struct timeval timeout_struct;

    memset(&timeout_struct, 0, sizeof(timeout_struct));
    if (timeout != -1) { timeout_struct.tv_usec = 1000 * timeout; }

    FD_ZERO(&fdset);
    FD_SET(conn->fd, &fdset);
    if (select(conn->fd + 1, for_read ? &fdset : NULL,
        for_read ? NULL: &fdset, NULL, timeout == -1 ? NULL : &timeout) < 0)
    { 
        strerror_r(errno, conn->error_message, sizeof(conn->error_message));
        return 1; 
    }

    return 0;
}

int connection_close(connection_t *conn) {
    if ((close(conn->fd) == -1)) {
        strerror_r(errno, conn->error_message, sizeof(conn->error_message));
        return 1;
    }
    return 0;
}
