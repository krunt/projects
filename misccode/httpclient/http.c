#include "http.h"
#include "error.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

/* #define DEBUG */

static int calculate_request_size(request_t *request, char *hostport, char *path) {
    return sizeof(REQUEST_FORMAT_STR) - 2 - 2 
      + (request->proxy ? strlen(request->url) + strlen(request->proxy)
                        : strlen(path) + strlen(hostport));
}

static int send_request(request_t *request) {
    int err = 0, request_str_size;
    char *request_str, *scheme, *hostport, *path = "/";
    char *url_copy = request->aops->strdup(request->pool, (char*)request->url);

    if ((err = http_parse_url(url_copy, &scheme, &hostport, &path))) {
        set_request_error_message(request, err, request->url);
        goto free_2;
    }

    request_str_size = calculate_request_size(request, hostport, path);
    request_str = request->aops->alloc(request->pool, request_str_size);
 
    snprintf(request_str, request_str_size, REQUEST_FORMAT_STR, 
             request->proxy ? request->url : path,
             request->proxy ? request->proxy : hostport);
    err = connection_write(&request->conn, request_str, request_str_size);

    request->aops->free(request->pool, request_str);
free_2:
    request->aops->free(request->pool, url_copy);
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
    const int bufsize = 4096;
    char buf[bufsize+1], *buf_end;

    err = 0;
    while (!err) {
        err = connection_poll(response->conn, 1, response->timeout);

        while (!err) {
            err = connection_read(response->conn, buf, bufsize);
            if (err == CONNECTION_READ_ERROR) goto error;
            else if (err == CONNECTION_READ_EOF) return 0;

            buf_end = buf;
            size_left = size = err;
            err = 0;
            while (!err && buf_end != buf + size) {

#ifdef DEBUG
                {
                    fprintf(stderr, "\n\n\nSTATE<%d,%d,%d,%d>: ```%s````\n", 
                            response->state, size_left, 
                            response->chunked_read_state, 
                            response->chunked_bytes_left,
                            buf_end);
                }
#endif

                 switch (response->state) {
                 case RESPONSE_STATUS_LINE:
                     err = response->cb->process_status_line(response, &buf_end, 
                             &size_left);
                 break;
         
                 case RESPONSE_PARSE_HEADERS:
                     err = response->cb->process_headers(response, &buf_end, 
                             &size_left);
                 break;
         
                 case RESPONSE_GET_BODY:
                     err = response->cb->process_body(response, &buf_end, &size_left);
                 break;

                 case RESPONSE_GET_BODY_CHUNKED:
                     err = response->cb->process_body_chunked(response, 
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
    do { *capacity = !*capacity ? 8 : *capacity << 1; } while (*capacity < needed);
    *buf = response->req->aops->realloc(response->req->pool, *buf, *capacity);
    return 0;
}

static int parse_num(const char *numstr, int *num, int base) {
    int result;
    char *end_ptr = (char*)numstr;
    *num = strtol(numstr, &end_ptr, base);
    return end_ptr == numstr || errno == ERANGE;
}

static int is_response_failure(response_t *response) {
    return (response->status_code >= 400 && response->status_code <= 599);
}

static int parse_response_status_line(response_t *response) {
    int code;
    char *p = response->status_line_buf;
    char *end = p + response->status_line_size;

    /* skipping whitespace */
    while (p < end && *p == ' ') { p++; }

    /* skipping http-version */
    while (p < end && *p != ' ') { p++; }

    /* skipping whitespace */
    while (p < end && *p == ' ') { p++; }

    if (parse_num(p, &code, 10)
            || code < 100 || code > 599)
    { return 1; }

    response->status_code = code;
    if (is_response_failure(response)) {
        set_request_error_message(response->req, RESPONSE_BAD_STATUS_CODE, code);
        return 1;
    }

    return 0;
}

static int response_process_status_line(response_t *response, 
        char **buf, int *size) 
{
    int err = 0, to_copy;
    char *end, end_this_state = 0;
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
    memcpy(response->status_line_buf + response->status_line_size, *buf, to_copy);
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
        while (p < end && (*p == ' ' || *p == '\t')) { ++p; }
        header_key_begin = p;

        /* reading token */
        while (p < end && !notoken_symbols[(int)(unsigned char)*p]) { ++p; }

        if (p == end || *p != ':') {
            set_request_error_message(response->req, RESPONSE_HEADER_IS_INVALID, 
                    header_key_begin);
            return 1;
        }
        *p++ = 0;

        while (p < end && (*p == ' ' || *p == '\t')) { ++p; }

        header_value_begin = p;
        while (p < end && !(*(p-1) == CR && *p == LF)) { ++p; }

        *p++ = 0;

#ifdef DEBUG
                {
                    fprintf(stderr, "`%s`: `%s`\n", 
                            header_key_begin, header_value_begin);
                }
#endif

        if (!strcasecmp(header_key_begin, "Transfer-Encoding")
            && !strncasecmp(header_value_begin, "chunked", sizeof("chunked") - 1))
        { response->chunked_transfer_encoding = 1; }
        else if (!strcasecmp(header_key_begin, "Content-Length")) {
            if (parse_num(header_value_begin, &response->content_length, 10)) { 
                set_request_error_message(response->req, CONTENT_LENGTH_IS_INVALID,
                        header_value_begin);
                return 1; 
            }
        } else if (!strcasecmp(header_key_begin, "Location")) {
            response->location = header_value_begin;
        }
    }

    if (response->chunked_transfer_encoding && response->content_length) {
        set_request_error_message_noarg(response->req, RESPONSE_LENGTH_CLASH);
        return 1;
    }

    return 0;
}

static int is_redirect(response_t *response) {
    return (response->status_code == 301 
        || response->status_code == 302
        || response->status_code == 303
        || response->status_code == 307)
        && response->location;
}

int response_process_headers(response_t *response, char **buf, int *size) {
    int err, to_copy;
    char *end, end_this_state = 0;
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
    memcpy(response->headers_buf + response->headers_buf_size, *buf, to_copy);
    response->headers_buf_size += to_copy;

    if (end_this_state) {
        if ((err = parse_response_headers(response))) {
            return err;
        }

        if (is_redirect(response))
            return REQUEST_NEED_REDIRECT;

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
        response->fops->write(response->body_fd, *buf, *size);
    } else {
        enlarge_string_if_needed(response, &response->body_buf, 
            &response->body_capacity, 
            *size + response->body_size);
        memcpy(response->body_buf + response->body_size, *buf, *size);
    }

    response->body_written_bytes += *size;
    if (response->body_written_bytes >= response->content_length) {
        response->state = RESPONSE_PARSE_SUCCESSFUL;
    }

    *buf += *size;
    *size = 0;

    return err;
}

int response_process_body_chunked(response_t *response, char **buf, int *size) {
    int err = 0, to_copy, end_this_state = 0, removed_bytes = 0;
    char *p, *end;
    assert(*size > 0);

    switch (response->chunked_read_state) {
    case CHUNKED_READ_SIZE: {
        (*buf)[*size] = 0;

        /* need to skip some bytes CRLF from last chunk */
        while (**buf && (**buf == CR || **buf == LF)) { 
            (*buf)++; 
            removed_bytes++; 
        }

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
        memcpy(response->chunked_size_buf + response->chunked_size_buf_size, 
                *buf, to_copy);

        if (end_this_state) {
            response->chunked_read_state = CHUNKED_READ_BODY;

            if (parse_num(response->chunked_size_buf, 
                        &response->chunked_bytes_left, 16)) {
                return 1;
            }

            /* last chunk */
            if (!response->chunked_bytes_left) {
                response->state = RESPONSE_PARSE_SUCCESSFUL;
            }
        }

        *buf += to_copy;
        *size -= to_copy + removed_bytes;
        break;
    }

    case CHUNKED_READ_BODY: {
        to_copy = MIN(response->chunked_bytes_left, *size);

        if (response->body_filename) {
            response->fops->write(response->body_fd, *buf, to_copy);
        } else {
            enlarge_string_if_needed(response, &response->body_buf, 
                &response->body_capacity, 
                to_copy + response->body_size);
            memcpy(response->body_buf + response->body_size, *buf, to_copy);
        }

        response->chunked_bytes_left -= to_copy;
        if (response->chunked_bytes_left <= 0) {
            response->chunked_read_state = CHUNKED_READ_SIZE;
        }
    
        *buf += to_copy;
        *size -= to_copy;
        break;
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
        const char *proxy, const char *output_filename) 
{
    int err, http_port;
    char *url_hostport, *scheme, *hostport, *path, *http_host;
    char *url_copy;

    memset(request, 0, sizeof(*request));
    request->aops = &malloc_ops;

    url_copy = request->aops->strdup(request->pool, (void *)url);

    request->url = request->aops->strdup(request->pool, (void *)url);
    request->proxy = proxy;
    request->output_filename = output_filename;

    if ((err = http_parse_url(url_copy, &scheme, &hostport, &path))) {
        request->aops->free(request->pool, url_copy);
        set_request_error_message(request, err, request->url);
        return 1;
    }

    if ((err = http_parse_hostport((char*)(proxy ? proxy : hostport), 
                    &http_host, &http_port))) {
        request->aops->free(request->pool, url_copy);
        set_request_error_message(request, err, hostport);
        return 1;
    }
    
    if (connection_open(&request->conn, http_host, http_port, 
            request->error_message, sizeof(request->error_message))) {
        request->aops->free(request->pool, url_copy);
        return 1;
    }

    request->pool = NULL;

    request->aops->free(request->pool, url_copy);
    return 0;
}

int reinit_request(request_t *request, response_t *response) {
    assert(is_redirect(response));
    close_request(request);
    return init_request(request, response->location, request->proxy, 
            request->output_filename);
}

int init_response(request_t *request, response_t *response) {
    int reinit = request == response->req;

    if (!reinit) {
        memset(response, 0, sizeof(*response));
    }

    response->req = request;
    response->conn = &request->conn;
    response->state = RESPONSE_STATUS_LINE;

    response->status_line_size = 0;
    response->headers_buf_size = 0;
    response->status_line_size = 0;
    response->body_size = 0;
    response->chunked_size_buf_size = 0;

    if (!reinit) {
        response->status_line_buf = NULL;
        response->status_line_capacity = 0;
    
        response->headers_buf = NULL;
        response->headers_buf_capacity = 0;
    
        response->body_buf = NULL;
        response->body_capacity = 0;

        response->chunked_size_buf = NULL;
        response->chunked_size_buf_capacity = 0;
    }

    response->chunked_read_state = CHUNKED_READ_SIZE;
    response->chunked_bytes_left = 0;

    response->fops = &general_file_operations;
    response->cb = &general_http_parse_callbacks;

    response->body_filename = request->output_filename;
    response->body_written_bytes = 0;

    /* there is a need to open file */
    if (!reinit && response->body_filename
        && (response->body_fd = response->fops->open(response->body_filename,
                O_CREAT | O_WRONLY)) < 0)
    {
        strerror_r(errno, request->error_message, sizeof(request->error_message));
        return 1;
    }

    response->status_code = 0;
    response->chunked_transfer_encoding = 0;
    response->content_length = 0;
    response->location = NULL;

    response->timeout = -1; /* infinite */

    return 0;
}

int close_request(request_t *request) {
    connection_close(&request->conn);
    request->aops->free(request->pool, (void*)request->url);
    request->url = NULL;
    return 0;
}

int close_response(response_t *response) {
    if (response->status_line_buf) {
        response->req->aops->free(response->req->pool, response->status_line_buf);
    }
    if (response->headers_buf) {
        response->req->aops->free(response->req->pool, response->headers_buf);
    }
    if (response->body_buf) {
        response->req->aops->free(response->req->pool, response->body_buf);
    }
    if (response->chunked_size_buf) {
        response->req->aops->free(response->req->pool, response->chunked_size_buf);
    }
    if (response->body_filename) {
        response->fops->close(response->body_fd);
    }
    response->req = 0;
    return 0;
}


int process_request(request_t *request, response_t *response) {
    int err = 0;
    do {
        if (err = init_response(request, response))
            break;

        err = process_request_internal(request, response);
        if (err == REQUEST_NEED_REDIRECT) {
            if (reinit_request(request, response))
                return 1;
        }
    } while (err == REQUEST_NEED_REDIRECT);
    return err;
}

