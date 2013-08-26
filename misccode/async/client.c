#include "packet.h"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int connect_to_peer(const char *vhost, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        return fd;
    }
    struct sockaddr_in saddr;
    memset((void *)&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = inet_addr(vhost);
    if (connect(fd, (const struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
        return -1;
    }
    return fd;
}

int main() {
    int fd;
    if ((fd = connect_to_peer("127.0.0.1", PORT)) == -1)
        return -1;
    struct packet_header header = { 0, htonl(4) };
    if (write(fd, (void *)&header, sizeof(header)) != sizeof(header)
            || write(fd, (void *)"hi!", 4) != 4) {
        return -2;
    }
    return 0;
}
