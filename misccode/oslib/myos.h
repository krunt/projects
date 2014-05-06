#ifndef MYOS_DEF_H
#define MYOS_DEF_H

#ifdef WIN32
#include "myos_win32.h"
#else
#include "myos_unix.h"
#endif

/*

fdsocket_t
fdhandle_t

*/

struct sockaddr;

typedef struct myos_s {
    void (*init)(void);

    int (*get_last_error)(void);
    void (*get_last_error_message)(char *buf, int buflen);

    int (*socket_create)(fdsocket_t *s, int af, int type, int protocol);
    int (*socket_connect)(fdsocket_t s, const struct sockaddr *addr, int addrlen);
    int (*socket_bind)(fdsocket_t s, const struct sockaddr *addr, int addrlen);
    int (*socket_accept)(fdsocket_t s, 
            struct sockaddr *addr, int *addrlen,
            fdsocket_t *accepted_socket);
    int (*socket_read)(fdsocket_t s, char *buf, int len);
    int (*socket_write)(fdsocket_t s, const char *buf, int len);
    int (*socket_close)(fdsocket_t s);
    struct hostent *(*socket_gethostbyname)(const char *name);

    fdhandle_t (*file_open)(const char *fname, int mode);
    int (*file_read)(fdhandle_t fd, char *buf, int len);
    int (*file_write)(fdhandle_t fd, const char *buf, int len);
    int (*file_close)(fdhandle_t fd);
} myos_t;

#ifdef WIN32
extern myos_t myos_win32;
#else
extern myos_t myos_unix;
#endif

myos_t *myos();

#endif /* MYOS_DEF_H */
