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
    int err, request_str_size;
    char *request_str, *scheme, *hostport, *path = "/";
    char *url_copy = request->aops.strdup(request->pool, request->url);

    if (http_parse_url(url_copy, &scheme, &hostport, &path)) {
        set_request_error_message(request, err = URI_IS_INVALID, request->url);
        goto free_2;
    }

    if (strcmp(scheme, "http")) { 
        set_request_error_message(request, err = SCHEME_IS_NOT_SUPPORTED, "scheme");
        goto free_2;
    }

    request_str_size = calculate_request_size(request, hostport, path);
    request_str = request->aops.alloc(request->pool, request_str_size);
 
    snprintf(request_str, request_str_size, REQUEST_FORMAT_STR, 
             request->proxy ? request->uri : path,
             request->proxy ? request->proxy : hostport);
 
    if (!(err = make_request(request, request_str))) {
        err = -1;
    } else { 
        set_request_error_message(request, err); 
    }

free_1:
    request->aops.free(request->pool, request_str);
free_2:
    request->aops.free(request->pool, url_copy);
    return err + 1;
}

static int process_request_internal(request_t *request, response_t *response) {
    int err;

    if ((err = send_request(request))) {
        return err;
    }

    init_response(request, response);
    retrieve_response(request, response);
}

int retrieve_response(request_t *request, response_t *response) {
    int size, err, read_bytes, size_left;
    char buf[4096], buf_end;

    err = 0;
    while (!err) {
        err = connection_poll(response->conn, 0 /* poll for read */);

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
                 case RESPONSE_GET_BODY_CHUNKED:
                     err = response->cb.body(response, &buf_end, &size_left);
                 break;
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

int response_process_status_line(response_t *response, char **buf, int *size) {
    char *end, end_this_state, to_copy;
    assert(*size > 0);

    (*buf)[*size] = 0;
    end_this_state = 0;
    if ((end = strstr(*buf, CRLF CRLF))) {
        end[0] = end[1] = 0;
        end_this_state = 1;
    } else {
        end = *buf + *size;
    }

    to_copy = end - *buf;
    enlarge_string_if_needed(response, &response->status_line_buf, 
            &response->status_line_capacity, 
            to_copy + response->status_line_size);
    strncpy(status_line_buf + response->status_line_size, *buf, to_copy);
    response->status_line_size += to_copy;

    if (end_this_state) {
        response->state = RESPONSE_PARSE_HEADERS;
    }

    *buf += to_copy;
    *size -= to_copy;
}

int process_request(request_t *request, response_t *response) {
    int err;
    while ((err = process_request_internal(request, response))
            && err == REQUEST_NEED_REDIRECT) {}
    return err;
}
