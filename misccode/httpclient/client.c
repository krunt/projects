#include "http.h"

int main(int argc, char **argv) {
    request_t request;
    init_request(&request);
    if (process_request(&request, &response)) {
        fprintf(stderr, "%s\n", request.error_message);
        close_request(&request);
        return 1;
    }
    close_request(&request);
    return 0;
}
