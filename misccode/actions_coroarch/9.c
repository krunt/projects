#include <stdio.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/socket.h>

#include "libasync.h"

static int is_blocking(int fd) {
    return fcntl(fd, F_GETFL) & O_NONBLOCK; 
}

ssize_t (*orig_read_func)(int fd, void *buf, size_t count);
ssize_t (*orig_write_func)(int fd, const void *buf, size_t count);
size_t (*orig_fread_func)(void *ptr, size_t size,
        size_t n, FILE *stream);
size_t (*orig_fwrite_func)(const void *ptr, size_t size,
        size_t n, FILE *stream);
ssize_t (*orig_sendto_func)(int fd, const void *buf, size_t n,
    int flags, const struct sockaddr *addr, socklen_t addr_len);
ssize_t (*orig_recvfrom_func)(int fd, void *buf, size_t n,
    int flags, struct sockaddr *addr, socklen_t *addr_len);
ssize_t (*orig_send_func)(int fd, const void *buf, size_t len, int flags);
ssize_t (*orig_recv_func)(int fd, void *buf, size_t len, int flags);

extern ssize_t read(int fd, void *buf, size_t count) {
    ssize_t res;
    if (is_blocking(fd)) {
        async_wait_fd(fd, READ_FLAG);
    }
    res = orig_read_func(fd, buf, count);
    fprintf(stderr, "read(%d, %p, %ld) = %ld\n", fd, buf, count, res);
    return res;
}

extern ssize_t write(int fd, const void *buf, size_t count) {
    ssize_t res;
    if (is_blocking(fd)) {
        async_wait_fd(fd, WRITE_FLAG);
    }
    res = orig_write_func(fd, buf, count);
    fprintf(stderr, "write(%d, %p, %ld) = %ld\n", fd, buf, count, res);
    return res;
}

extern size_t fread(void *ptr, size_t size,
        size_t n, FILE *stream)
{
    size_t res;
    if (is_blocking(fileno(stream))) {
        async_wait_fd(fileno(stream), READ_FLAG);
    }
    res = orig_fread_func(ptr, size, n, stream);
    fprintf(stderr, "fread(%d, %p, %ld) = %ld\n", fileno(stream), ptr, size, res);
    return res;
}

extern size_t fwrite(const void *ptr, size_t size,
        size_t n, FILE *stream)
{
    size_t res;
    if (is_blocking(fileno(stream))) {
        async_wait_fd(fileno(stream), WRITE_FLAG);
    }
    res = orig_fwrite_func(ptr, size, n, stream);
    fprintf(stderr, "fwrite(%d, %p, %ld) = %ld\n", fileno(stream), ptr, size, res);
    return res;
}

extern ssize_t sendto(int fd, const void *buf, size_t n,
    int flags, const struct sockaddr *addr, socklen_t addr_len)
{
    size_t res;
    if (is_blocking(fd)) {
        async_wait_fd(fd, WRITE_FLAG);
    }
    res = orig_sendto_func(fd, buf, n, flags, addr, addr_len);
    fprintf(stderr, "sendto(%d, %p, %ld) = %ld\n", fd, buf, n, res);
    return res;
}

extern ssize_t recv(int fd, void *buf, size_t len, int flags)
{
    size_t res;
    if (is_blocking(fd)) {
        async_wait_fd(fd, READ_FLAG);
    }
    res = orig_recv_func(fd, buf, len, flags);
    fprintf(stderr, "recv(%d, %p, %ld) = %ld\n", fd, buf, len, res);
    return res;
}

extern ssize_t send(int fd, const void *buf, size_t len, int flags)
{
    size_t res;
    if (is_blocking(fd)) {
        async_wait_fd(fd, WRITE_FLAG);
    }
    res = orig_send_func(fd, buf, len, flags);
    fprintf(stderr, "send(%d, %p, %ld) = %ld\n", fd, buf, len, res);
    return res;
}

extern ssize_t recvfrom(int fd, void *buf, size_t n,
    int flags, struct sockaddr *addr, socklen_t *addr_len)
{
    size_t res;
    if (is_blocking(fd)) {
        async_wait_fd(fd, WRITE_FLAG);
    }
    res = orig_recvfrom_func(fd, buf, n, flags, addr, addr_len);
    fprintf(stderr, "recvfrom(%d, %p, %ld) = %ld\n", fd, buf, n, res);
    return res;
}

#define SETUP_FUNC(f,fname) \
    f = dlsym(dllib, #fname); \
    if (!f) { \
        fprintf(stderr, "no %s function found: %s\n", #fname, dlerror()); \
        return; \
    } \

static void __attribute__((constructor)) setup_libc_functions() {
    void *dllib = dlopen("libc.so.6", RTLD_NOW);

    if (!dllib) {
        fprintf(stderr, "no library found: %s\n", dlerror());
        return;
    }

    SETUP_FUNC(orig_read_func, read);
    SETUP_FUNC(orig_write_func, write);
    SETUP_FUNC(orig_fread_func, fread);
    SETUP_FUNC(orig_fwrite_func, fwrite);
    SETUP_FUNC(orig_sendto_func, sendto);
    SETUP_FUNC(orig_recvfrom_func, recvfrom);
    SETUP_FUNC(orig_send_func, send);
    SETUP_FUNC(orig_recv_func, recv);
}
