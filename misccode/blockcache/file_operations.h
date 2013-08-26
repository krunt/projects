#ifndef FILE_OPERATIONS__
#define FILE_OPERATIONS__

#include <unistd.h>

struct file_operations {
    ssize_t (*pread)(int fd, void *buf, size_t count, off_t offset);
    ssize_t (*pwrite)(int fd, const void *buf, size_t count, off_t offset);
};

void init_file_operations(struct file_operations *fop, int debug);

#endif /* FILE_OPERATIONS__ */
