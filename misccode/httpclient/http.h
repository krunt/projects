#ifndef HTTP_DEF_
#define HTTP_DEF_

const int REQUEST_NEED_REDIRECT = -1;
const int CONNECTION_READ_NOT_READY = -2;

/* response states */
const int RESPONSE_STATUS_LINE = 0;
const int RESPONSE_PARSE_HEADERS = 1;
const int RESPONSE_GET_BODY = 2;
const int RESPONSE_GET_BODY_CHUNKED = 3;

typedef struct alloc_operations_s {
    void (*alloc)(void *pool, size_t size);
    void (*realloc)(void *pool, char *p, size_t size);
    void (*strdup)(void *pool, char *p);
    void (*free)(void *pool, char *p);
} alloc_operations_t;

typedef struct connection_s {
    int fd;
} connection_t;

typedef struct request_s {
    const char *url;
    const char *proxy;

    char error_message[MAX_ERROR_MESSAGE_LEN];

    connection_t *conn;
    alloc_operations_t aops;

    void *pool; /* unused */
} request_t;

typedef struct response_s {
    request_t *req;
    connection_t *conn;

    int state;

    char *status_line_buf;
    int status_line_size;
    int status_line_capacity;

    char *headers_buf;
    int headers_buf_size;
    int headers_buf_capacity;

    char *body_buf;
    int body_size;
    int body_capacity;

    const char *body_filename;
    int body_fd;

    int status_code;
    int transfer_encoding;
    int content_length;

    response_parse_callbacks_t cb;
} response_t;

typedef struct response_parse_callbacks_s {
    int (*process_status_line)(response_t *response, char *buf, int size);
    int (*process_headers)(response_t *response, char *buf, int size);
    int (*process_body)(response_t *response, char *buf, int size);
} response_parse_callbacks_t;

int init_request();
int init_response();
int free_request();
int free_response();

int process_request(request_t *request, response_t *response);
int make_request(request_t *request, const char *request_body);
int retrieve_response(request_t *request, response_t *response);

#endif /* HTTP_DEF_ */
