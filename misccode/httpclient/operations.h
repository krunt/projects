#ifndef HTTP_OPERATIONS_DEF__
#define HTTP_OPERATIONS_DEF__

typedef struct file_operations_s {
    int (*open)(const char *filename, int flags);
    int (*read)(int fd, char *buf, int size);
    int (*write)(int fd, char *buf, int size);
    int (*close)(int fd);
} file_operations_t;

typedef struct alloc_operations_s {
    void *(*alloc)(void *pool, int size);
    void *(*realloc)(void *pool, void *p, int size);
    void *(*strdup)(void *pool, void *p);
    void (*free)(void *pool, void *p);
} alloc_operations_t;

typedef struct response_parse_callbacks_s {
    int (*process_status_line)(response_t *response, char *buf, int size);
    int (*process_headers)(response_t *response, char *buf, int size);
    int (*process_body)(response_t *response, char *buf, int size);
    int (*process_body_chunked)(response_t *response, char *buf, int size);
} response_parse_callbacks_t;

#endif /* HTTP_OPERATIONS_DEF__ */
