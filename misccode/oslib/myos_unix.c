
#include "myos.h"

void unix_init(void) {}
int unix_get_last_error(void) { return errno; }
void unix_get_last_error_message(char *buf, int buflen) {
    strerror_r(errno, buf, buflen);
}
int unix_socket_create(fdsocket_t *s, int af, int type, int protocol) { 
    *s = socket(af, type, protocol);
    return *s < 0 ? 1 : 0;
}
int unix_socket_connect(fdsocket_t s, const struct sockaddr *addr, int addrlen) {
    return connect(s, addr, addrlen) == -1 ? 1 : 0;
}
int unix_socket_bind(fdsocket_t s, const struct sockaddr *addr, int addrlen) {
    return bind(s, addr, addrlen) == -1 ? 1 : 0;
}
int unix_socket_accept(fdsocket_t s, struct sockaddr *addr, int *addrlen,
    fdsocket_t *accepted_socket) 
{
    *accepted_socket = accept(s, addr, addrlen);
    return *accepted_socket == -1 ? 1 : 0;
}
int unix_socket_read(fdsocket_t s, char *buf, int len) {
    return read(s, buf, len);
}
int unix_socket_write(fdsocket_t s, const char *buf, int len) {
    return write(s, buf, len);
}
int unix_socket_close(fdsocket_t s) {
    close(s);
    return 0;
}
struct hostent *unix_socket_gethostbyname(const char *name) {
    return gethostbyname(name);
}
int unix_file_open(fdhandle_t *fd, const char *fname, int mode) {
    *fd = open(fname, mode, 0644);
    return *fd == -1 ? 1 : 0;
}

int unix_file_read(fdhandle_t s, char *buf, int len) {
    return read(s, buf, len);
}
int unix_file_write(fdhandle_t s, const char *buf, int len) {
    return write(s, buf, len);
}
int unix_file_close(fdhandle_t s) {
    close(s);
    return 0;
}

myos_t myos_unix = {
    .init = &unix_init,
    .get_last_error = &unix_get_last_error,
    .get_last_error_message = &unix_get_last_error_message,
    .socket_create = &unix_socket_create,
    .socket_connect = &unix_socket_connect,
    .socket_bind = &unix_socket_bind,
    .socket_accept = &unix_socket_accept,
    .socket_read = &unix_socket_read,
    .socket_write = &unix_socket_write,
    .socket_close = &unix_socket_close,
    .socket_gethostbyname = &unix_socket_gethostbyname,
    .file_open = &unix_file_open,
    .file_read = &unix_file_read,
    .file_write = &unix_file_write,
    .file_close = &unix_file_close,
};
