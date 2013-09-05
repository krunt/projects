#include "http.h"

#define CR '\r'
#define LF '\n'
#define CRLF "\r\n"
#define REQUEST_FORMAT_STR \
    "GET %s HTTP/1.1" CRLF \
    "Host: %s" CRLF \
    "User-Agent: tiny_krunt_client" CRLF \
    CRLF

#define set_request_error_message(request, error_code, ...) \
    snprintf(request->error_message, sizeof(request->error_message), \
            error_message_formats[error_code], __VA_ARGS__);

static int calculate_request_size(request_t *request, char *hostport, char *path) {
    return sizeof(REQUEST_FORMAT_STR) - 2 - 2 
        request->proxy  ?  strlen(request->uri) + strlen(request->proxy)
                        :  strlen(path) + strlen(hostport);
}

static int send_request(request_t *request) {
    int err = 0, request_str_size;
    char *request_str, *scheme, *hostport, *path = "/";
    char *url_copy = request->aops.strdup(request->pool, request->url);

    if (http_parse_url(url_copy, &scheme, &hostport, &path)) {
        err = 1;
        set_request_error_message(request, URI_IS_INVALID, request->url);
        goto free_2;
    }

    if (strcmp(scheme, "http")) { 
        err = 1;
        set_request_error_message(request, SCHEME_IS_NOT_SUPPORTED, 
                "scheme");
        goto free_2;
    }

    request_str_size = calculate_request_size(request, hostport, path);
    request_str = request->aops.alloc(request->pool, request_str_size);
 
    snprintf(request_str, request_str_size, REQUEST_FORMAT_STR, 
             request->proxy ? request->uri : path,
             request->proxy ? request->proxy : hostport);
    err = connection_write(request->conn, request_str, request_str_size);

free_1:
    request->aops.free(request->pool, request_str);
free_2:
    request->aops.free(request->pool, url_copy);
    return err;
}

static int process_request_internal(request_t *request, response_t *response) {
    int err;
    if ((err = send_request(request))) {
        return err;
    }
    return retrieve_response(request, response);
}

int retrieve_response(request_t *request, response_t *response) {
    int size, err, read_bytes, size_left;
    char buf[4096], buf_end;

    err = 0;
    while (!err) {
        err = connection_poll(response->conn, 1 /* poll for read */);

        while (!err) {
            if ((err = connection_read(response->conn, buf, sizeof(buf), 
                            &size)) == CONNECTION_READ_NOT_READY)
            { break; }
            else if (err) {
                goto error;
            }

            buf_end = buf;
            size_left = size;
            while (buf_begin != buf + size) {
                 switch (response->state) {
                 case STATE_STATUS_LINE:
                     err = response->cb.process_status_line(response, &buf_end, 
                             &size_left);
                 break;
         
                 case RESPONSE_PARSE_HEADERS:
                     err = response->cb.process_headers(response, &buf_end, 
                             &size_left);
                 break;
         
                 case RESPONSE_GET_BODY:
                     err = response->cb.body(response, &buf_end, &size_left);
                 break;

                 case RESPONSE_GET_BODY_CHUNKED:
                     err = response->cb.body_chunked(response, 
                             &buf_end, &size_left);
                 break;

                 case RESPONSE_PARSE_SUCCESSFUL:
                    return 0;
                 };
            }
        }
    }

error:
    return err;
}

static int enlarge_string_if_needed(response_t *response, 
            char **buf, int *capacity, int needed) 
{
    if (*capacity >= needed) {
        return 0;
    }
    do { *capacity <<= 1; } while (*capacity < needed);
    *buf = response->req.aops.realloc(response->req.pool, *capacity);
    return 0;
}

static int parse_num(const char *numstr, int *num, int base) {
    int result;
    char *end_ptr = numstr;
    *num = strtol(numstr, &end_ptr, base);
    return end_ptr == numstr || *num == LONG_MAX || *num == LONG_MIN;
}

static int parse_response_status_line(response_t *response) {
    int code;
    char *p;
    p = response->status_line_buf;

    /* skipping http-version */
    while (*p && *p != ' ') { p++; }

    /* skipping whitespace */
    while (*p && *p != ' ') { p++; }

    if (parse_num(response->status_line_buf, &code, 10)
            || code < 100 || code > 599)
    { return 1; }

    response->status_code = code;
    return 0;
}

static int response_process_status_line(response_t *response, char **buf, int *size) {
    int err = 0;
    char *end, end_this_state, to_copy;
    assert(*size > 0);

    (*buf)[*size] = 0;
    end_this_state = 0;
    if ((end = strstr(*buf, CRLF))) {
        end[0] = end[1] = 0;
        end_this_state = 1;
        end += 2;
    } else {
        end = *buf + *size;
    }

    to_copy = end - *buf;
    enlarge_string_if_needed(response, &response->status_line_buf, 
            &response->status_line_capacity, 
            to_copy + response->status_line_size);
    strncpy(response->status_line_buf + response->status_line_size, *buf, to_copy);
    response->status_line_size += to_copy;

    if (end_this_state) {
        if ((err = parse_response_status_line(response))) {
            return err;
        }
        response->state = RESPONSE_PARSE_HEADERS;
    }

    *buf += to_copy;
    *size -= to_copy;

    return err;
}

static const int notoken_symbols[256] = {
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
};

static int parse_response_headers(response_t *response) {
    char *end_ptr;
    char *p = response->headers_buf;
    char *end = response->headers_buf + response->headers_buf_size;
    char *header_key_begin, *header_value_begin;
    while (p < end) {
        /* skipping possible whitespace */
        while (p < end && *p == ' ' || *p == '\t') { ++p; }

        while (p < end && *p == ' ' || *p == '\t') { ++p; }
        header_key_begin = p;

        /* reading token */
        while (p < end && !notoken_symbols[(int)(unsigned char)*p]) { ++p; }

        if (p == end || *p != ':') {
            return 1;
        }
        *p++ = 0;

        while (p < end && *p == ' ' || *p == '\t') { ++p; }

        header_value_begin = p;
        while (p < end && !(*(p-1) == CR && *p == LF)) { ++p; }
        *p++ = 0;

        if (!strcasecmp(header_key_begin, "Transfer-Encoding")
            && !strcasecmp(header_value_begin, "chunked"))
        { response->chunked_transfer_encoding = 1; }
        else if (!strcasecmp(header_key_begin, "Content-Length")) {
            if (parse_num(header_value_begin, &response->content_length, 10))
            { return 1; }
        }
    }

    if (response->chunked_transfer_encoding && repose->content_length) {
        return 1;
    }

    return 0;
}

int response_process_headers(response_t *response, char **buf, int *size) {
    int err;
    char *end, end_this_state, to_copy;
    assert(*size > 0);

    (*buf)[*size] = 0;
    end_this_state = 0;
    if ((end = strstr(*buf, CRLF CRLF))) {
        end[0] = end[1] = end[2] = end[3] = 0;
        end_this_state = 1;
        end += 4;
    } else {
        end = *buf + *size;
    }

    to_copy = end - *buf;
    enlarge_string_if_needed(response, &response->headers_buf, 
            &response->headers_buf_capacity, 
            to_copy + response->headers_buf_size);
    strncpy(response->headers_buf + response->headers_buf_size, *buf, to_copy);
    response->headers_buf_size += to_copy;

    if (end_this_state) {
        if ((err = parse_response_headers(response))) {
            return err;
        }
        response->state 
            = response->chunked_transfer_encoding ? RESPONSE_GET_BODY_CHUNKED
                    : RESPONSE_GET_BODY;
    }

    *buf += to_copy;
    *size -= to_copy;

    return err;
}

int response_process_body(response_t *response, char **buf, int *size) {
    int err = 0;
    assert(*size > 0);

    if (response->body_filename) {
        response->fop.write(response->body_fd, *buf, *size);
    } else {
        enlarge_string_if_needed(response, &response->body_buf, 
            &response->body_capacity, 
            *size + response->body_size);
        memcpy(response->body_buf, *buf, *size);
    }

    response->body_written_bytes += *size;
    if (response->body_written_bytes >= reponse->content_length) {
        response->state = RESPONSE_PARSE_SUCCESSFUL;
    }

    *buf += *size;
    *size = 0;

    return err;
}

int response_process_body_chunked(response_t *response, char **buf, int *size) {
    int err = 0, to_copy, end_this_state;
    char *p, *end;
    assert(*size > 0);

    switch (response->chunked_read_state) {
    case CHUNKED_READ_SIZE: {
        (*buf)[*size] = 0;
        if ((end = strstr(*buf, CRLF))) {
            end[0] = end[1] = 0;
            end_this_state = 1;
            end += 2;
        } else {
            end = *buf + *size;
        }
        to_copy = end - *buf;

        enlarge_string_if_needed(response, &response->chunked_size_buf, 
            &response->chunked_size_buf_capacity, 
            to_copy + response->chunked_size_buf_size);
        memcpy(response->chunked_size_buf + to_copy, *buf, to_copy);

        if (end_this_state) {
            response->chunked_read_state = CHUNKED_READ_BODY;

            /* need to skip some bytes CRLF from last chunk */
            p = response->chunked_size_buf;
            while (*p && (p == CR || p == LF)) { p++; }

            if (parse_num(p, &response->chunked_bytes_left, 16)) {
                return 1;
            }

            /* last chunk */
            if (!response->chunked_bytes_left) {
                response->state = RESPONSE_PARSE_SUCCESSFUL;
            }
        }

        *buf += to_copy;
        *size += to_copy;
        return 0;
    }

    case CHUNKED_READ_BODY: {
        to_copy = MIN(response->chunked_bytes_left, *size);

        if (response->body_filename) {
            response->fop.write(response->body_fd, *buf, to_copy);
        } else {
            enlarge_string_if_needed(response, &response->body_buf, 
                &response->body_capacity, 
                to_copy + response->body_size);
            memcpy(response->body_buf + response->body_size, *buf, to_copy);
        }

        response->chunked_bytes_left -= to_copy;
        if (!response->chunked_bytes_left) {
            response->chunked_read_state = CHUNKED_READ_SIZE;
        }
    
        *buf += to_copy
        *size -= to_copy;
    }};

    return err;
}

static response_parse_callbacks_t general_http_parse_callbacks = {
    .process_status_line = &response_process_status_line,
    .process_headers = &response_process_headers,
    .process_body = &response_process_body,
    .process_body_chunked = &response_process_body_chunked,
};


int init_request(request_t *request, const char *url, 
        const char *proxy = NULL, const char *output_filename = NULL) 
{
    char *conn_host; 
    int conn_port;
    char *url_hostport, *scheme, *path;
    char *url_copy = request->aops.strdup(request->pool, request->url);

    memset(request, 0, sizeof(*request));
    request->url = url;
    request->proxy = proxy;
    request->output_filename = output_filename;

    if (http_parse_url(url_copy, &scheme, &hostport, &path)) {
        request->aops.free(request->pool, url_copy);
        set_request_error_message(request, URI_IS_INVALID, request->url);
        return 1;
    }

    if (http_parse_hostport(proxy ? proxy : hostport, &http_host, &http_port)) {
        request->aops.free(request->pool, url_copy);
        set_request_error_message(request, HOSTPORT_IS_INVALID, hostport);
        return 1;
    }
    
    if (connection_open(&request->conn, http_host, http_port, 
            request->error_message, sizeof(request->error_message))) {
        request->aops.free(request->pool, url_copy);
        return 1;
    }

    request->aops = &malloc_ops;
    request->pool = NULL;

    request->aops.free(request->pool, url_copy);
    return 0;
}

int init_response(request_t *request, response_t *response) {
    memset(response, 0, sizeof(*response));
    response->req = request;
    response->conn = &request->conn;
    response->state = STATE_STATUS_LINE;

    response->status_line_buf = NULL;
    response->status_line_size = 0;
    response->status_line_capacity = 0;

    response->headers_buf = NULL;
    response->headers_buf_size = 0;
    response->headers_buf_capacity = 0;

    response->body_buf = NULL;
    response->body_size = 0;
    response->body_capacity = 0;

    response->chunked_read_state = CHUNKED_READ_SIZE;
    response->chunked_size_buf = NULL;
    response->chunked_size_buf_size = 0;
    response->chunked_size_buf_capacity = 0;
    response->chunked_bytes_left = 0;

    response->fops = &general_file_operations;
    response->cb = &general_http_parse_callbacks;

    response->body_filename = request->output_filename;
    response->body_written_bytes = 0;

    /* there is a need to open file */
    if (response->body_filename
        && (response->body_fd = response->fops->open(response->body_filename,
                O_WRONLY)) < 0)
    {
        strerror_r(errno, request->error_message, sizeof(request->error_message));
        return 1;
    }

    response->status_code = 0;
    response->chunked_transfer_encoding = 0;
    response->content_length = 0;

    return 0;
}

int close_request(request_t *request) {
    connection_close(&request->conn);
    return 0;
}

int close_response(response_t *response) {
    response->req = 0;
    if (response->status_line_buf) {
        response->req->aops->free(response->status_line_buf);
    }
    if (response->headers_buf) {
        response->req->aops->free(response->headers_buf);
    }
    if (response->body_buf) {
        response->req->aops->free(response->body_buf);
    }
    if (response->chunked_size_buf) {
        response->req->aops->free(response->chunked_size_buf);
    }
    if (response->body_filename) {
        response->fops->close(response->body_fd);
    }
    return 0;
}


int process_request(request_t *request, response_t *response) {
    int err;

    int err = 0;
    do {
        if (err = init_response(request, response))
            break;

        err = process_request_internal(request, response);
        if (err != REQUEST_NEED_REDIRECT) {
            reinit_request(request, response);
            close_response(response);
        }
    } while (err == REQUEST_NEED_REDIRECT);
    return err;
}

