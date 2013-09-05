
#define MAX_ERROR_MESSAGE_LEN 4096

extern const char *error_messages_formats[];

const int URI_IS_INVALID = 0;
const int SCHEME_IS_NOT_SUPPORTED = 1;
const int HOSTPORT_IS_INVALID = 2;
const int RESPONSE_HEADER_IS_INVALID = 3;
const int CONTENT_LENGTH_IS_INVALID = 4;
const int RESPONSE_LENGTH_CLASH = 5;
