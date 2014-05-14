#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define MYOS_EAGAIN WSAEWOULDBLOCK
#define MYOS_EINTR WSAEINTR

typedef SOCKET fdsocket_t;
typedef HANDLE fdhandle_t;
