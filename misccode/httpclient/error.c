#include "error.h"

const char *error_messages_formats[] = {
    "Url `%s` parse error", /* URI_IS_INVALID */
    "Scheme http is only supported", /* SCHEME_IS_NOT_SUPPORTED */
    "HostPort `%s` parse error", /* HOSTPORT_IS_INVALID */
    "Response header is invalid didn't find `:` or end  in `%s`", 
        /* RESPONSE_HEADER_IS_INVALID  */
    "Invalid numeric in Content-Length value", 
        /* CONTENT_LENGTH_IS_INVALID */
    "Invalid response: both transfer-encoding and content-length specified", 
    /* RESPONSE_LENGTH_CLASH  */
    "Http port is invalid in url `%s`", /* RESPONSE_HTTP_PORT_IS_INVALID  */
    "Http response code `%d`", /* RESPONSE_BAD_STATUS_CODE  */
};


