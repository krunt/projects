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

#define MYOS_READ 0 
#define MYOS_WRITE 1 
#define MYOS_CREAT 2 
#define MYOS_APPEND 4

struct sockaddr;

typedef struct myos_s {
    void (*init)(void);
    void (*deinit)(void);

    int (*get_last_error)(void);
    void (*get_last_error_message)(char *buf, int buflen);

    int (*socket_create)(fdsocket_t *s, int af, int type, int protocol);
    int (*socket_connect)(fdsocket_t s, const struct sockaddr *addr, int addrlen);
    int (*socket_bind)(fdsocket_t s, const struct sockaddr *addr, int addrlen);
    int (*socket_listen)(fdsocket_t s, int backlog);
    int (*socket_accept)(fdsocket_t s, 
            struct sockaddr *addr, int *addrlen,
            fdsocket_t *accepted_socket);
    int (*socket_getsockopt)(fdsocket_t s, int level, int optname,
            void *optval, socklen_t *optlen);
    int (*socket_setsockopt)(fdsocket_t s, int level, int optname,
            const void *optval, socklen_t optlen);
    int (*socket_read)(fdsocket_t s, char *buf, int len);
    int (*socket_write)(fdsocket_t s, const char *buf, int len);
    int (*socket_recv)(fdsocket_t s, char *buf, int len);
    int (*socket_send)(fdsocket_t s, const char *buf, int len);
    int (*socket_close)(fdsocket_t s);
    int (*socket_select)(int nfds, fd_set *rfds, fd_set *wfds, fd_set *excfds,
            struct timeval *timeout);
    struct hostent *(*socket_gethostbyname)(const char *name);
    void (*socket_set_blocking)(fdsocket_t s, int blocking);


    unsigned short (*socket_htons)(unsigned short);
    unsigned int (*socket_htonl)(unsigned int);
    unsigned short (*socket_ntohs)(unsigned short);
    unsigned int (*socket_ntohl)(unsigned int);

    int (*file_open)(fdhandle_t *fd, const char *fname, int mode);
    int (*file_read)(fdhandle_t fd, char *buf, int len);
    int (*file_write)(fdhandle_t fd, const char *buf, int len);
    int (*file_close)(fdhandle_t fd);
    void (*file_set_blocking)(fdhandle_t fd, int blocking);

    int (*gettimeofday)(struct timeval *tv);
    void (*sleep)(int usec);

} myos_t;

#ifdef WIN32
extern myos_t myos_win32;
#else
extern myos_t myos_unix;
#endif

myos_t *myos();

#endif /* MYOS_DEF_H */
