#ifndef HTTP_DEF_
#define HTTP_DEF_

#include "connection.h"

const int REQUEST_NEED_REDIRECT = -1;
const int CONNECTION_READ_NOT_READY = -2;

/* response states */
const int RESPONSE_STATUS_LINE = 0;
const int RESPONSE_PARSE_HEADERS = 1;
const int RESPONSE_GET_BODY = 2;
const int RESPONSE_GET_BODY_CHUNKED = 3;
const int RESPONSE_PARSE_SUCCESSFUL = 4;

/* chunked fetch states */
const int CHUNKED_READ_SIZE = 1;
const int CHUNKED_READ_BODY = 2;

typedef struct request_s {
    const char *url;
    const char *proxy;
    const char *output_filename;

    char error_message[MAX_ERROR_MESSAGE_LEN];

    connection_t conn;
    alloc_operations_t *aops;

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

    int chunked_read_state;
    char *chunked_size_buf;
    int chunked_size_buf_size;
    int chunked_size_buf_capacity;
    int chunked_bytes_left;

    file_operations_t *fops;
    response_parse_callbacks_t cb;

    const char *body_filename;
    int body_fd;
    int body_written_bytes;

    int status_code;
    int chunked_transfer_encoding;
    int content_length;
} response_t;

int init_request(request_t *request);
int close_request(request_t *request);
int close_response(response_t *response);
int process_request(request_t *request, response_t *response);

#endif /* HTTP_DEF_ */
