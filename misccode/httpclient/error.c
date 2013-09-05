#include "error.h"

const char *error_messages_formats[] = {
    "Url `%s` parse error", /* URI_IS_INVALID */
    "Scheme %s is not supported", /* SCHEME_IS_NOT_SUPPORTED */
    "HostPort `%s` parse error", /* HOSTPORT_IS_INVALID */
    "Response header is invalid didn't find `:` or end", 
        /* RESPONSE_HEADER_IS_INVALID  */
    "Invalid numeric in Content-Length value", 
        /* CONTENT_LENGTH_IS_INVALID */
    "Invalid response: both transfer-encoding and content-length specified", 
    /* RESPONSE_LENGTH_CLASH  */
};


