#include "operations.h"

int open_wrapper(fdhandle_t *fd, const char *filename, int flags) { 
    return (*myos()->file_open)(fd, filename, flags);
}
int read_wrapper(fdhandle_t fd, char *buf, int size) { 
    return (*myos()->file_read)(fd, buf, size); 
}
int write_wrapper(fdhandle_t fd, char *buf, int size) { 
    return (*myos()->file_write)(fd, buf, size);
}
int close_wrapper(fdhandle_t fd) { 
    return (*myos()->file_close)(fd);
}

file_operations_t general_file_operations = {
    .open = &open_wrapper,
    .read = &read_wrapper,
    .write = &write_wrapper,
    .close = &close_wrapper,
};


