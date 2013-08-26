#include "file_operations.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

ssize_t pread_debug(int fd, void *buf, size_t count, off_t offset) {
    ssize_t r = pread(fd, buf, count, offset);
    if (r == -1) {
        /*
        char buf[128];
        strerror_r(errno, buf, sizeof(buf));
        fprintf(stderr, "pread: %s\n", buf);
        */
        fprintf(stderr, "pread: %d\n", errno);
        perror("");
    }
    return r;
}

ssize_t pwrite_debug(int fd, const void *buf, size_t count, off_t offset) {
    ssize_t r = pwrite(fd, buf, count, offset);
    if (r == -1) {
        /*
        fprintf(stderr, "pwrite(%d,%p,%d,%d)\n", fd, buf, count, offset);
        char buf[128];
        strerror_r(errno, buf, sizeof(buf));
        fprintf(stderr, "pwrite: %s\n", buf);
        */
        fprintf(stderr, "pwrite: %d\n", errno);
        perror("");
    }
    return r;
}

void
init_file_operations(struct file_operations *fop, int debug) {
    if (debug) {
        fop->pread = &pread_debug;
        fop->pwrite = &pwrite_debug;
    } else {
        fop->pread = &pread;
        fop->pwrite = &pwrite;
    }
}
