#include "http.h"

#include <stdio.h>

int main(int argc, char **argv) {
    int err = 0, err2 = 0;
    request_t request;
    response_t response;
    const char *url, *proxy, *filename;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <url> [<filename>]\n", argv[0]);
        return 1;
    }

    url = argv[1]; 
    proxy = (char *)getenv("proxy");
    filename = argc == 3 ? argv[2] : "default.out";
    if ((err2 = init_request(&request, url, proxy, filename))
            || process_request(&request, &response)) 
    {
        fprintf(stderr, "Error=`%s`\n", request.error_message);
        err = 1;
    }

    if (!err2) {
        close_response(&response);
        close_request(&request);
    }
    return err;
}
