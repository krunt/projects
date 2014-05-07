
#include "myos.h"

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
    return GetLastError();
}

int win32_get_last_socket_error(void) { 
    return WSAGetLastError();
}

void win32_get_last_error_message(char *buf, int buflen) {
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
            NULL, GetLastError(), 0, buf, buflen, NULL);
}
void win32_get_last_socket_error_message(char *buf, int buflen) {
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
            NULL, WSAGetLastError(), 0, buf, buflen, NULL);
}

int win32_socket_create(fdsocket_t *s, int af, int type, int protocol) { 
    *s = socket(af, type, protocol);
    return *s == INVALID_SOCKET ? 1 : 0;
}
int win32_socket_connect(fdsocket_t s, const struct sockaddr *addr, int addrlen) {
    return connect(s, addr, addrlen) == SOCKET_ERROR ? 1 : 0;
}
int win32_socket_bind(fdsocket_t s, const struct sockaddr *addr, int addrlen) {
    return bind(s, addr, addrlen) == SOCKET_ERROR ? 1 : 0;
}
int win32_socket_accept(fdsocket_t s, struct sockaddr *addr, int *addrlen,
    fdsocket_t *accepted_socket) 
{
    *accepted_socket = accept(s, addr, addrlen);
    return *accepted_socket == INVALID_SOCKET ? 1 : 0;
}
int win32_socket_getsockopt(fdsocket_t s, int level, int optname,
            void *optval, socklen_t *optlen)
{
    int rc = getsockopt(s, level, optname, optval, optlen);
    return rc == SOCKET_ERROR ? 1 : 0;
}
int win32_socket_setsockopt(fdsocket_t s, int level, int optname,
            void *optval, socklen_t optlen)
{
    int rc = setsockopt(s, level, optname, optval, optlen);
    return rc == SOCKET_ERROR ? 1 : 0;
}
int win32_socket_read(fdsocket_t s, char *buf, int len) {
    int rc = recv(s, buf, len, 0);
    if (rc == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return MYOS_EAGAIN;
        if (WSAGetLastError() == WSAEINTR)
            return MYOS_EINTR;
    }
    return rc;
}
int win32_socket_write(fdsocket_t s, const char *buf, int len) {
    int rc = send(s, buf, len, 0);
    if (rc == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return MYOS_EAGAIN;
        if (WSAGetLastError() == WSAEINTR)
            return MYOS_EINTR;
    }
    return rc;
}

int win32_socket_close(fdsocket_t s) {
    closesocket(s);
    return 0;
}

int win32_socket_select(int nfds, fd_set *rfds, fd_set *wfds, fd_set *excfds,
            struct timeval *timeout) 
{
    int rc = select(nfds, rfds, wfds, excfds, timeout);
    if (rc == SOCKET_ERROR && WSAGetLastError() == WSAEINTR)
        return MYOS_EINTR;
    return rc;
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

    res = CreateFile(fname, access_mode, share_mode, NULL,
        creation_mode, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (res == INVALID_HANDLE_VALUE) {
        return 1;
    }

    return 0;
}

int win32_file_read(fdhandle_t s, char *buf, int len) {
    DWORD bytesRead;
    BOOL rc = ReadFile(s, buf, len, &bytesRead, NULL);
    return !rc ? -1 : bytesRead;
}

int win32_file_write(fdhandle_t s, char *buf, int len) {
    DWORD bytesWritten;
    BOOL rc = WriteFile(s, buf, len, &bytesWritten, NULL);
    return !rc ? -1 : bytesWritten;
}

int win32_file_close(fdhandle_t fd) {
    CloseHandle(fd);
    return 0;
}

void win32_file_set_blocking(fdhandle_t s, int blocking) {
    /* ??? */
}

unsigned short win32_socket_htons(unsigned short n) { return htons(n); }
unsigned int win32_socket_htonl(unsigned int n) { return htonl(n); }
unsigned short win32_socket_ntohs(unsigned short n) { return ntohs(n); }
unsigned int win32_socket_ntohl(unsigned int n) { return ntohl(n); }

myos_t myos_win32 = {
    .init = &win32_init,
    .deinit = &win32_deinit,
    .get_last_error = &win32_get_last_error,
    .get_last_socket_error = &win32_get_last_socket_error,
    .get_last_error_message = &win32_get_last_error_message,
    .get_last_socket_error_message = &win32_get_last_socket_error_message,
    .socket_create = &win32_socket_create,
    .socket_connect = &win32_socket_connect,
    .socket_bind = &win32_socket_bind,
    .socket_accept = &win32_socket_accept,
    .socket_getsockopt = &win32_socket_getsockopt,
    .socket_setsockopt = &win32_socket_setsockopt,
    .socket_read = &win32_socket_read,
    .socket_write = &win32_socket_write,
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
};

