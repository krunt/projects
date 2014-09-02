
#include "myos.h"
#include <errno.h>

#pragma comment (lib, "Ws2_32.lib")

static int wsa_inited = 0;
static WSADATA data;
void win32_init(void) {
    int version = 0x2;
    WSADATA retdata;

    if (wsa_inited)
        return;

    WSAStartup(version, &retdata);
    wsa_inited = 1;
}

void win32_deinit(void) {
    if (!wsa_inited)
        return;

    WSACleanup();
    wsa_inited = 0;
}

int win32_get_last_error(void) { 
    return errno;
}

void win32_get_last_error_message(char *buf, int buflen) {
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
            NULL, errno, 0, buf, buflen, NULL);
}

#define CHECK_CALL(x, y) \
    if ((x) == (y)) { \
        errno = GetLastError(); \
    } 

#define CHECK_SOCKET_CALL(x, y) \
    if ((x) == (y)) { \
        errno = WSAGetLastError(); \
    } 

int win32_socket_create(fdsocket_t *s, int af, int type, int protocol) { 
    CHECK_SOCKET_CALL(*s = socket(af, type, protocol), INVALID_SOCKET);
    return *s == INVALID_SOCKET ? 1 : 0;
}
int win32_socket_connect(fdsocket_t s, const struct sockaddr *addr, int addrlen) {
    int rc;
    CHECK_SOCKET_CALL((rc = connect(s, addr, addrlen)), SOCKET_ERROR);
    return rc == SOCKET_ERROR ? 1 : 0;
}
int win32_socket_bind(fdsocket_t s, const struct sockaddr *addr, int addrlen) {
    int rc;
    CHECK_SOCKET_CALL((rc = bind(s, addr, addrlen)), SOCKET_ERROR);
    return rc == SOCKET_ERROR ? 1 : 0;
}
int win32_socket_listen(fdsocket_t s, int backlog) {
    int rc;
    CHECK_SOCKET_CALL((rc = listen(s, backlog)), SOCKET_ERROR);
    return rc == SOCKET_ERROR ? 1 : 0;
}
int win32_socket_accept(fdsocket_t s, struct sockaddr *addr, int *addrlen,
    fdsocket_t *accepted_socket) 
{
    CHECK_SOCKET_CALL(*accepted_socket = accept(s, addr, addrlen), INVALID_SOCKET);
    return *accepted_socket == INVALID_SOCKET ? 1 : 0;
}
int win32_socket_getsockopt(fdsocket_t s, int level, int optname,
            void *optval, socklen_t *optlen)
{
    int rc;
    CHECK_SOCKET_CALL(rc = getsockopt(s, level, optname, optval, optlen), SOCKET_ERROR);
    return rc == SOCKET_ERROR ? 1 : 0;
}
int win32_socket_setsockopt(fdsocket_t s, int level, int optname,
            void *optval, socklen_t optlen)
{
    int rc;
    CHECK_SOCKET_CALL(rc= setsockopt(s, level, optname, optval, optlen), SOCKET_ERROR);
    return rc == SOCKET_ERROR ? 1 : 0;
}
int win32_socket_read(fdsocket_t s, char *buf, int len) {
    int rc;
    CHECK_SOCKET_CALL(rc = recv(s, buf, len, 0), SOCKET_ERROR);
    return rc == SOCKET_ERROR ? -1 : rc;
}
int win32_socket_write(fdsocket_t s, const char *buf, int len) {
    int rc;
    CHECK_SOCKET_CALL(rc = send(s, buf, len, 0), SOCKET_ERROR);
    return rc == SOCKET_ERROR ? -1 : rc;
}

int win32_socket_close(fdsocket_t s) {
    int rc;
    CHECK_SOCKET_CALL(rc = closesocket(s), SOCKET_ERROR);
    return rc == SOCKET_ERROR ? 1 : 0;
}

int win32_socket_select(int nfds, fd_set *rfds, fd_set *wfds, fd_set *excfds,
            struct timeval *timeout) 
{
    int rc;
    CHECK_SOCKET_CALL(rc = select(nfds, rfds, wfds, excfds, timeout), SOCKET_ERROR);
    return rc == SOCKET_ERROR ? -1 : rc;
}

struct hostent *win32_socket_gethostbyname(const char *name) {
    return gethostbyname(name);
}

void win32_socket_set_blocking(fdsocket_t s, int blocking) {
    u_long arg = blocking ? 0 : 1;
    ioctlsocket(s, FIONBIO, &arg);
}

int win32_file_open(fdhandle_t *fd, const char *fname, int mode) 
{
    fdhandle_t res;
    int access_mode, creation_mode, share_mode;

    access_mode = creation_mode = share_mode = 0;

    if (mode & MYOS_READ) access_mode |= GENERIC_READ;
    if (mode & MYOS_WRITE) access_mode |= GENERIC_WRITE;

    if (!(mode & MYOS_WRITE)) {
        share_mode |= FILE_SHARE_READ;
        creation_mode |= OPEN_EXISTING;
    }

    if (mode & MYOS_APPEND) access_mode |= FILE_APPEND_DATA;

    if (mode & MYOS_CREAT) {
        creation_mode |= CREATE_NEW;
    }

    CHECK_CALL(res = CreateFile(fname, access_mode, share_mode, NULL,
        creation_mode, FILE_ATTRIBUTE_NORMAL, NULL), INVALID_HANDLE_VALUE);
    return res == INVALID_HANDLE_VALUE ? 1 : 0;
}

int win32_file_read(fdhandle_t s, char *buf, int len) {
    BOOL rc;
    DWORD bytesRead;
    CHECK_CALL(rc = ReadFile(s, buf, len, &bytesRead, NULL), FALSE);
    return !rc ? -1 : bytesRead;
}

int win32_file_write(fdhandle_t s, char *buf, int len) {
    BOOL rc;
    DWORD bytesWritten;
    CHECK_CALL(rc = WriteFile(s, buf, len, &bytesWritten, NULL), FALSE);
    return !rc ? -1 : bytesWritten;
}

int win32_file_close(fdhandle_t fd) {
    int rc;
    CHECK_CALL(rc = CloseHandle(fd), FALSE);
    return rc ? 0 : 1;
}

void win32_file_set_blocking(fdhandle_t s, int blocking) {
    /* ??? */
}

unsigned short win32_socket_htons(unsigned short n) { return htons(n); }
unsigned int win32_socket_htonl(unsigned int n) { return htonl(n); }
unsigned short win32_socket_ntohs(unsigned short n) { return ntohs(n); }
unsigned int win32_socket_ntohl(unsigned int n) { return ntohl(n); }

#define EPOCH_BIAS  116444736000000000LL
int win32_gettimeofday(struct timeval *tv) {
    FILETIME ft;
    ULARGE_INTEGER g_64;

    GetSystemTimeAsFileTime(&ft);

    g_64.u.LowPart = ft.dwLowDateTime;
    g_64.u.HighPart = ft.dwHighDateTime;

    tv->tv_sec = (long)((g_64.QuadPart - EPOCH_BIAS) / 10000000LL);
    tv->tv_usec = (long)((g_64.QuadPart / 10LL) % 1000000LL);

    return 0;
}

void win32_sleep(int usec) {
    Sleep(usec);
}

myos_t myos_win32 = {
    .init = &win32_init,
    .deinit = &win32_deinit,
    .get_last_error = &win32_get_last_error,
    .get_last_error_message = &win32_get_last_error_message,
    .socket_create = &win32_socket_create,
    .socket_connect = &win32_socket_connect,
    .socket_bind = &win32_socket_bind,
    .socket_listen = &win32_socket_listen,
    .socket_accept = &win32_socket_accept,
    .socket_getsockopt = &win32_socket_getsockopt,
    .socket_setsockopt = &win32_socket_setsockopt,
    .socket_read = &win32_socket_read,
    .socket_write = &win32_socket_write,
    .socket_recv = &win32_socket_read,
    .socket_send = &win32_socket_write,
    .socket_close = &win32_socket_close,
    .socket_select = &win32_socket_select,
    .socket_gethostbyname = &win32_socket_gethostbyname,
    .socket_set_blocking = &win32_socket_set_blocking,

    .socket_htons = win32_socket_htons,
    .socket_htonl = win32_socket_htons,
    .socket_ntohs = win32_socket_ntohs,
    .socket_ntohl = win32_socket_ntohl,

    .file_open = &win32_file_open,
    .file_read = &win32_file_read,
    .file_write = &win32_file_write,
    .file_close = &win32_file_close,
    .file_set_blocking = &win32_file_set_blocking,

    .gettimeofday = &win32_gettimeofday,
    .sleep = &win32_sleep,
};

