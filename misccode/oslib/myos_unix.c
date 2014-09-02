
#include "myos.h"

#define MYOS_EAGAIN EAGAIN
#define MYOS_EINTR  EINTR

void unix_init(void) {}
void unix_deinit(void) {}
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
int unix_socket_listen(fdsocket_t s, int backlog) {
    return listen(s, backlog) ? 1 : 0;
}
int unix_socket_accept(fdsocket_t s, struct sockaddr *addr, int *addrlen,
    fdsocket_t *accepted_socket) 
{
    *accepted_socket = accept(s, addr, addrlen);
    return *accepted_socket == -1 ? 1 : 0;
}
int unix_socket_getsockopt(fdsocket_t s, int level, int optname,
            void *optval, socklen_t *optlen)
{
    int rc = getsockopt(s, level, optname, optval, optlen);
    return rc == -1 ? 1 : 0;
}
int unix_socket_setsockopt(fdsocket_t s, int level, int optname,
            void *optval, socklen_t optlen)
{
    int rc = setsockopt(s, level, optname, optval, optlen);
    return rc == -1 ? 1 : 0;
}
int unix_socket_read(fdsocket_t s, char *buf, int len) {
    int rc = read(s, buf, len);
    return rc;
}
int unix_socket_write(fdsocket_t s, const char *buf, int len) {
    int rc = write(s, buf, len);
    return rc;
}
int unix_socket_recv(fdsocket_t s, char *buf, int len) {
    int rc = recv(s, buf, len, 0);
    return rc;
}
int unix_socket_send(fdsocket_t s, const char *buf, int len) {
    int rc = send(s, buf, len, 0);
    return rc;
}
int unix_socket_close(fdsocket_t s) {
    close(s);
    return 0;
}
int unix_socket_select(int nfds, fd_set *rfds, fd_set *wfds, fd_set *excfds,
            struct timeval *timeout) 
{
    return select(nfds, rfds, wfds, excfds, timeout);
}
struct hostent *unix_socket_gethostbyname(const char *name) {
    return gethostbyname(name);
}
void unix_socket_set_blocking(fdsocket_t s, int blocking) {
    int flags = fcntl(s, F_GETFL, 0);
    if (blocking) { flags &= ~O_NONBLOCK; }
    else { flags  |= O_NONBLOCK; }
    fcntl(s, F_SETFL, flags);
}
int unix_file_open(fdhandle_t *fd, const char *fname, int mode) {
    int open_mode = 0;

    if (mode & MYOS_READ) open_mode |= O_RDONLY;
    if (mode & MYOS_WRITE) open_mode |= O_WRONLY;
    if (mode & MYOS_CREAT) open_mode |= O_CREAT;
    if (mode & MYOS_APPEND) open_mode |= O_APPEND;

    *fd = open(fname, open_mode, 0644);
    return *fd == -1 ? 1 : 0;
}

int unix_file_read(fdhandle_t s, char *buf, int len) {
    int rc = read(s, buf, len);
    return rc;
}
int unix_file_write(fdhandle_t s, const char *buf, int len) {
    int rc = write(s, buf, len);
    return rc;
}
int unix_file_close(fdhandle_t s) {
    close(s);
    return 0;
}
void unix_file_set_blocking(fdhandle_t s, int blocking) {
    int flags = fcntl(s, F_GETFL, 0);
    if (blocking) { flags &= ~O_NONBLOCK; }
    else { flags  |= O_NONBLOCK; }
    fcntl(s, F_SETFL, flags);
}

unsigned short unix_socket_htons(unsigned short n) { return htons(n); }
unsigned int unix_socket_htonl(unsigned int n) { return htonl(n); }
unsigned short unix_socket_ntohs(unsigned short n) { return ntohs(n); }
unsigned int unix_socket_ntohl(unsigned int n) { return ntohl(n); }

int unix_gettimeofday(struct timeval *tv) {
    if (gettimeofday(tv, NULL) == -1)
        return 1;
    return 0;
}

void unix_sleep(int usec) {
    usleep(usec);
}

myos_t myos_unix = {
    .init = &unix_init,
    .get_last_error = &unix_get_last_error,
    .get_last_error_message = &unix_get_last_error_message,
    .socket_create = &unix_socket_create,
    .socket_connect = &unix_socket_connect,
    .socket_bind = &unix_socket_bind,
    .socket_listen = &unix_socket_listen,
    .socket_accept = &unix_socket_accept,
    .socket_getsockopt = &unix_socket_getsockopt,
    .socket_setsockopt = &unix_socket_setsockopt,
    .socket_read = &unix_socket_read,
    .socket_write = &unix_socket_write,
    .socket_recv = &unix_socket_recv,
    .socket_send = &unix_socket_send,
    .socket_close = &unix_socket_close,
    .socket_select = &unix_socket_select,
    .socket_gethostbyname = &unix_socket_gethostbyname,
    .socket_set_blocking = &unix_socket_set_blocking,

    .socket_htons = unix_socket_htons,
    .socket_htonl = unix_socket_htons,
    .socket_ntohs = unix_socket_ntohs,
    .socket_ntohl = unix_socket_ntohl,

    .file_open = &unix_file_open,
    .file_read = &unix_file_read,
    .file_write = &unix_file_write,
    .file_close = &unix_file_close,
    .file_set_blocking = &unix_file_set_blocking,

    .gettimeofday = unix_gettimeofday,
    .sleep = unix_sleep,
};
