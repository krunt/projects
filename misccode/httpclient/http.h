#ifndef HTTP_DEF_
#define HTTP_DEF_

#include "connection.h"
#include "operations.h"

#define CR '\r'
#define LF '\n'
#define CRLF "\r\n"
#define REQUEST_FORMAT_STR \
    "GET %s HTTP/1.1" CRLF \
    "Host: %s" CRLF \
    "User-Agent: tiny_krunt_client" CRLF \
    CRLF

#define set_request_error_message_noarg(request, error_code) \
    strcpy((request)->error_message, error_messages_formats[(error_code)]);

#define set_request_error_message(request, error_code, ...) \
    snprintf((request)->error_message, sizeof(request->error_message), \
            error_messages_formats[(error_code)], __VA_ARGS__);

#define REQUEST_NEED_REDIRECT -1

/* response states */
#define RESPONSE_STATUS_LINE 0
#define RESPONSE_PARSE_HEADERS 1
#define RESPONSE_GET_BODY 2
#define RESPONSE_GET_BODY_CHUNKED 3
#define RESPONSE_PARSE_SUCCESSFUL 4

/* chunked fetch states */
#define CHUNKED_READ_SIZE 1
#define CHUNKED_READ_BODY 2

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
    response_parse_callbacks_t *cb;

    const char *body_filename;
    int body_fd;
    int body_written_bytes;

    int status_code;
    int chunked_transfer_encoding;
    int content_length;
    char *location;

    int timeout; /* seconds */

} response_t;

int init_request(request_t *request, const char *url, 
        const char *proxy, const char *output_filename);
int close_request(request_t *request);
int close_response(response_t *response);
int process_request(request_t *request, response_t *response);

#endif /* HTTP_DEF_ */
