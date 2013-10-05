#include "client_priv.h"

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <violite.h>

#include <event.h>

/* constants */
const char *k_mysql_server = "127.0.0.1";
const int k_proxy_listen_port = 19999;
const int k_mysql_server_port = 3306;

enum kFlags {
    HANDSHAKE_BEFORE_SSL,
    HANDSHAKE_AFTER_SSL,
    HANDSHAKE_COMMAND_PHASE,
};

typedef struct acceptor_s {
    int fd;
    struct event event;
} acceptor_t;

typedef struct connector_s {
    int client_fd;
    struct event client_event;
    Vio *client_vio;
    int client_state;
    mysql_packet client_packet;

    int server_fd;
    struct event server_event;
    Vio *server_vio;
    int server_state;
    mysql_packet server_packet;
} connector_t;

void set_nonblockable(int fd) {
    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | O_NONBLOCK);
}

int create_listener_socket(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        return fd;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        return -1;
    }

    if (listen(fd, 127) == -1) {
        return -1;
    }

    set_nonblockable(fd);

    const int one = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&one, sizeof(one)) == -1) {
        return -1;
    }
    
    return fd;
}

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

static void
regenerate_acceptor_event(acceptor_t *acceptor) {
    event_add(&acceptor->event, NULL);
}

static void
regenerate_connector_event(connector_t *connector) {
    event_add(&connector->client_event, NULL);
    event_add(&connector->server_event, NULL);
}

static void
finalize_connector(connector_t *connector) {
    event_del(&connector->client_event);
    event_del(&connector->server_event);
    close(connector->server_fd);
    close(connector->client_fd);
    free((void*)connector);
}

#define MAX_PACKET_LENGTH (256L*256L*256L-1)
#define MIN(A,B) ((A) < (B) ? (A) : (B))
#define MAX(A,B) ((A) > (B) ? (A) : (B))

class mysql_packet {
public:
    mysql_packet() : str(NULL), bytes_left(0), is_last_packet(false) { 
        init_dynamic_string(str, "", 128, 16); 
    }

    ~mysql_packet()
    { dynstr_free(str); }

    bool empty() const { return str->length == 0; }
    bool is_ready() const { return bytes_left == 0 && is_last_packet; }

    uchar *append(uchar *data, int size) {
        size_t bytes_cnt;
        if (new_fragment()) {
            size_t part_length = bytes_left = uint3korr(data);
            is_last_packet = bytes_left != MAX_PACKET_LENGTH;
            data += 4; size -= 4; // TODO
        }
        bytes_cnt = MIN(size, bytes_left);
        dynstr_append_mem(str, data, bytes_cnt);
        bytes_left -= bytes_cnt;
        return data + bytes_cnt;
    }

    void clear() {
        str->length = 0;
        bytes_left = 0;
        is_last_packet = false;
    }

private:
    bool new_fragment() const {
        return bytes_left == 0;
    }

    DYNAMIC_STRING *str;
    size_t bytes_left;
    bool is_last_packet;
};

static void
server_connector_processor(connector_t *connector) {
    mysql_packet &packet = connect->server_packet;
}

static void
server_connector_handler(int fd, short event, void *arg) {
    size_t was_read; uchar buf[1024];
    connector_t *connector = (connector_t *)arg;
    if ((was_read = vio_read(connector->server_vio, buf, sizeof(buf))) != (size_t)-1) {
        mysql_packet &packet = connector->server_packet;
        uchar *new_buf = packet.append(buf, was_read);
        while (packet.is_ready()) {
            server_connector_processor(connector);
            packet.clear();
            new_buf = packet.append(new_buf, was_read - (new_buf - buf));
        }
        event_add(&connector->server_event, NULL);
    } else {
        finalize_connector(connector);
    }
}

static void
client_connector_handler(int fd, short event, void *arg) {
    size_t was_read; uchar buf[1024];
    connector_t *connector = (connector_t *)arg;
    if ((was_read = vio_read(connector->client_vio, buf, sizeof(buf))) != (size_t)-1) {
        vio_write(connector->server_vio, buf, was_read);
        event_add(&connector->client_event, NULL);
    } else {
        finalize_connector(connector);
    }
}

static void 
acceptor_handler(int fd, short event, void *arg) {
    acceptor_t *acceptor = (acceptor_t *)arg;
    int peer_fd = accept(acceptor->fd, NULL, NULL);
    if (peer_fd < 0) {
        return;
    }

    connector_t *connector = (connector_t *)malloc(sizeof(connector_t));
    memset((void *)connector, 0, sizeof(connector_t));
    set_nonblockable(peer_fd);
    connector->client_fd = peer_fd;

    int server_fd;
    if ((server_fd = connect_to_peer(k_mysql_server, k_mysql_server_port)) >= 0) {
        set_nonblockable(server_fd);
        connector->server_fd = server_fd; 

        connector->client_vio = vio_new(connector->client_fd, VIO_TYPE_TCPIP, 1);
        connector->server_vio = vio_new(connector->server_fd, VIO_TYPE_TCPIP, 1);

        connector->client_state = HANDSHAKE_BEFORE_SSL;
        connector->server_state = HANDSHAKE_BEFORE_SSL;

        event_set(&connector->client_event, connector->client_fd, 
            EV_READ, client_connector_handler, (void *)connector);
        event_set(&connector->server_event, connector->server_fd, 
            EV_READ, server_connector_handler, (void *)connector);
        regenerate_connector_event(connector);
    } else {
        close(peer_fd);
        free(connector);
    }
    regenerate_acceptor_event(acceptor);
}

int
initialize_acceptor(acceptor_t *acceptor, int port) {
    acceptor->fd = create_listener_socket(port);
    if (acceptor->fd == -1) {
        return -1;
    }
    event_set(&acceptor->event, acceptor->fd, 
                EV_READ | EV_PERSIST, acceptor_handler, (void *)acceptor);
    event_add(&acceptor->event, NULL);
    return acceptor->fd;
}

int main(int argc, char **argv) {
	MY_INIT(argv[0]);

    event_init();

    acceptor_t acceptor;
    if (initialize_acceptor(&acceptor, k_proxy_listen_port) < 0) {
        return 1;
    }

    event_dispatch();
    return 0;
}
