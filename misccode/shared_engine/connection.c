#include "connection.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include <strings.h>
#include <linux/un.h>

void 
pid2name(int pid, char *buf) {
    sprintf(buf, SOCKET_PREFIX, pid);
}

int
init_socket(state_t state) {
    struct sockaddr_un addr;

    int fd;
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    char unix_path[128] = { 0 };
    pid2name(state->myid.pid, unix_path);

    unlink(unix_path);
    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, unix_path, strlen(unix_path)+1);

    if (bind(fd, (struct sockaddr *)&addr, strlen(unix_path) + 1 + sizeof(addr.sun_family))
            || listen(fd, 128) < 0) 
    { return -1; }

    return fd;
}

int
init_connection(state_t state, int pid) {
    int fd;
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    char unix_path[128] = { 0 };
    pid2name(pid, unix_path);

    struct sockaddr_un addr;
    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, unix_path, strlen(unix_path) + 1);

    if (connect(fd, (struct sockaddr *)&addr, 
            strlen(unix_path) + 1 + sizeof(addr.sun_family)) == -1) {
        return -1;
    }

    return fd;
}

int 
init_connections(state_t state) {
    for (int i = 0; i < MAX_PIDS; ++i) {
        if (state->ids[i].pid == INVALID)
            continue;
        if (state->ids[i].pid == state->myid.pid)
            continue;

        int pid = state->ids[i].pid;
        state->ids[i].fd = init_connection(state, pid);
    }
    return 0;
}

int
close_connection(state_t state, int pid) {
    for (int i = 0; i < MAX_PIDS; ++i) {
        if (state->ids[i].pid == pid) {
            close(state->ids[i].fd);
            return 0;
        }
    }
    return -1;
}
