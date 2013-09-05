#include "operations.h"

int open_wrapper(const char *filename, int flags) { return open(filename, flags); }
int read_wrapper(int fd, char *buf, int size) { return read(fd, buf, size); }
int write_wrapper(int fd, char *buf, int size) { return write(fd, buf, size); }
int close_wrapper(int fd) { return close(fd); }

static file_operations_t general_file_operations = {
    .open = &open_wrapper,
    .read = &read_wrapper,
    .write = &write_wrapper,
    .close = &close_wrapper,
};


