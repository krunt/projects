#include "utils.h"
#include "error.h"

#include <stdio.h>
#include <string.h>

int http_parse_url(char *url, char **scheme, char **hostport, char **path) {
    char *pc, c;

    if (strncasecmp("http://", url, 7)) {
        return SCHEME_IS_NOT_SUPPORTED;
    }

    url[4] = 0;
    url += 6;

    memmove(url, url + 1, strlen(url + 1));
    url[strlen(url) - 1] = 0;
    for (pc = url, c = *pc; (c && c != '/');) c = *pc++;
    *(pc-1)=0;

    *hostport = url;
    if (c == '/') {
        *pc = '/';
        memmove(pc + 1, pc, strlen(pc));
        *path = pc + 1;
    }

    return 0;
}

int http_parse_hostport(char *hostport, char **host, int *port) {
    char *pc, c;
    for (pc = hostport, c = *pc; (c && c != ':');) c = *pc++;
    *(pc-1)=0;

    if (c==':') {
        if (sscanf(pc, "%d", port) != 1) {
            return RESPONSE_HTTP_PORT_IS_INVALID;
        }
    } else {
        *port = 80;
    }
    *host = hostport;
    return 0;
}
