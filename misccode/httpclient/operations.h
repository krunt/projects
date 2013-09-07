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

struct response_s;
typedef struct response_parse_callbacks_s {
    int (*process_status_line)(struct response_s *response, char **buf, int *size);
    int (*process_headers)(struct response_s *response, char **buf, int *size);
    int (*process_body)(struct response_s *response, char **buf, int *size);
    int (*process_body_chunked)(struct response_s *response, char **buf, int *size);
} response_parse_callbacks_t;

extern alloc_operations_t malloc_ops;
extern file_operations_t general_file_operations;

#endif /* HTTP_OPERATIONS_DEF__ */
