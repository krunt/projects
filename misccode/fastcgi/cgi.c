#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

/* remove connection object */ 

#include <event2/event.h>

#include "fastcgi.h"
#include "myclib/all.h"

#define get1(p) (unsigned char)*p++
#define get2(p) ((((unsigned char)*p)<<8) + ((unsigned char)*(p+1))), p+=2
#define get3(p) \
    ((((unsigned char)*p)<<16) + (((unsigned char)*(p+1))<<8) + \
        (((unsigned char)*(p+2)))), p+=3
#define get4(p) \
    ((((unsigned char)*p)<<24) + (((unsigned char)*(p+1))<<16) + \
        (((unsigned char)*(p+2))<<8) + ((unsigned char)*(p+3))), p+=4

static inline char *put1(char *p, int val) {
    *((unsigned char *)p) = val;
    return p + 1;
}
static inline char *put2(char *p, int val) {
    *((unsigned char *)p) = (val & 0xFF00) >> 8;
    *((unsigned char *)(p+1)) = val & 0xFF;
    return p + 2;
}
static inline char *put4(char *p, int val) {
    *((unsigned char *)p) = (val & 0xFF000000) >> 24;
    *((unsigned char *)(p+1)) = (val & 0x00FF0000) >> 16;
    *((unsigned char *)(p+2)) = (val & 0x0000FF00) >> 8;
    *((unsigned char *)(p+3)) = (val & 0x000000FF);
    return p + 4;
}

typedef struct fcgi_header_s {
    int version;
    int type;
    int request_id;
    int content_length;
    int padding_length;
} fcgi_header_t;

typedef struct fcgi_begin_request_body_s {
    int role;
    int flags;
} fcgi_begin_request_body_t;

typedef struct fcgi_begin_request_record_s {
    fcgi_header_t header;
    fcgi_begin_request_body_t body;
} fcgi_begin_request_record_t;

typedef struct fcgi_end_request_body_s {
    int app_status;
    int proto_status;
} fcgi_end_request_body_t;

typedef struct fcgi_end_request_record_s {
    fcgi_header_t header;
    fcgi_end_request_body_t body;
} fcgi_end_request_record_t;

typedef struct my_worker_s {
    int pid;
} my_worker_t;

typedef struct my_master_state_s {
    int listen_fd;
    int listen_port;

    int (*worker_cycle_proc)(void *);

    int workers;
    my_worker_t *workers_obj;

    const char *fastcgi_path;

} my_master_state_t;

typedef struct my_worker_state_s {
    struct event_base *evbase;
    struct event *evlisten;
    my_master_state_t *master_state;
    struct list_head conns;
} my_worker_state_t;

enum my_worker_request_state_t {
    req_reading_header,
    req_reading_body,
    req_closing,
};

typedef struct key_value_s {
    struct list_head entry;
    char *key;
    int key_length;
    char *value;
    int value_length;
} key_value_t;

typedef struct my_buffer_s {
    struct list_head entry;
    char buf[4096];
    int offs;
    int len;
    int alloced; /* 0|1 */
    int for_write; /* 0|1 */
} my_buffer_t;

typedef struct my_buffer_pool_s {
    struct list_head freelist;
    int buf_count_allocated;
    int buf_count_limit;
    int for_write; /* 0|1 */
} my_buffer_pool_t;

typedef struct my_fcgi_stream_s {
    struct event *ev;
    my_buffer_t *buf;
    int active;
    struct list_head queue;
    int for_write; /* 0|1 */
} my_fcgi_stream_t;

struct my_worker_connection_s;
typedef struct my_worker_request_s {
    struct list_head entry;

    int id;

    struct my_worker_connection_s *conn;

    enum my_worker_request_state_t state;

    char *pending;
    char *pending_ptr;
    int to_read;
    int to_read_header;

    int keep_conn;

    int fcgi_pid;
    int fcgi_appstatus;
    int fcgi_stdin[2];
    int fcgi_stdout[2];
    int fcgi_stderr[2];

    my_fcgi_stream_t fcgi_stdin_stream;
    my_fcgi_stream_t fcgi_stdout_stream;
    my_fcgi_stream_t fcgi_stderr_stream;

    /* this is needed for sending stdout/stderr packets */
    my_buffer_t packet_header; 
    struct list_head packet_batch;

    struct list_head env;

    struct list_head terminate_callbacks;

} my_worker_request_t;

typedef int (*my_callback_proc_t)(void *);

typedef struct my_callback_s {
    struct list_head entry;
    my_callback_proc_t func;
    void *arg;
} my_callback_t;

enum my_worker_connection_state_t {
    conn_reading_header,
    conn_reading_body, 
    conn_processing_request,
    conn_ignoring_body,
};

enum my_worker_output_queue_state_t {
    conn_writing_none,
    conn_writing_queue,
    conn_writing_stdout_queue_begin,
    conn_writing_stdout_queue,
    conn_writing_stderr_queue_begin,
    conn_writing_stderr_queue,
};

typedef struct my_worker_connection_s {
    struct list_head entry;

    int fd;

    enum my_worker_connection_state_t state;
    my_worker_request_t *reqs[65536];
    int request_count;

    char pending[FCGI_HEADER_LEN];
    char *pending_ptr;
    my_worker_state_t *worker_state;
    fcgi_header_t pending_header;

    int ignore_to_read;
    int ignore_read;

    struct timeval rw_timeout;

    my_buffer_pool_t read_pool;
    my_buffer_pool_t write_pool;

    int read_event_active;
    struct event *read_event;

    int write_event_active;
    struct event *write_event;

    enum my_worker_output_queue_state_t output_queue_state;
    struct list_head output_queue;
    
    int active_write_reqid;

    struct list_head waiting_for_freebuf_cbs_for_read;
    struct list_head waiting_for_freebuf_cbs_for_write;

    struct list_head waiting_for_freebuf_for_read;
    struct list_head waiting_for_freebuf_for_write;

} my_worker_connection_t;

static void my_buffer_pool_init(my_buffer_pool_t *pool, 
        int buffer_count, int for_write) 
{
    INIT_LIST_HEAD(&pool->freelist);
    pool->buf_count_allocated = 0;
    pool->buf_count_limit = buffer_count;
    pool->for_write = for_write;
}

static void my_buffer_pool_destroy(my_buffer_pool_t *pool) {
    my_buffer_t *b, *tmp;
    list_for_each_entry_safe(b, tmp, &pool->freelist, entry) {
        free(b);
    }
}

static my_buffer_t *my_get_buffer(my_buffer_pool_t *pool) {
    my_buffer_t *buf;

    if (!list_empty(&pool->freelist)) {
        buf = list_first_entry(&pool->freelist, my_buffer_t, entry);

        my_log_debug("found buffer %p in freelist", buf);

        list_del(pool->freelist.next);
        goto success;
    }

    if (pool->buf_count_allocated < pool->buf_count_limit) 
    {
        buf = malloc(sizeof(my_buffer_t));

        my_log_debug("allocating additional buffer %p", buf);

        if (!buf) {
            my_log_error("malloc() failed");
            return NULL;
        }

        buf->alloced = 1;
        buf->for_write = pool->for_write;

        ++pool->buf_count_allocated;

        goto success;
    }

    my_log_debug("buffer is not found");

    return NULL;

success:
    INIT_LIST_HEAD(&buf->entry);
    buf->offs = 0;
    buf->len = 0;

    assert(buf->alloced);
    
    return buf;
}

static void my_put_buffer(my_buffer_t *b, my_buffer_pool_t *pool) {
    list_add(&b->entry, &pool->freelist);
}

static my_callback_t *my_get_callback(my_callback_proc_t func, void *arg) {
    my_callback_t *cb = malloc(sizeof(my_callback_t));

    if (!cb)
        return NULL;

    INIT_LIST_HEAD(&cb->entry);
    cb->func = func;
    cb->arg = arg;

    return cb;
}

static int my_buffer_is_full(my_buffer_t *buf)
{
    return buf->len == sizeof(buf->buf);
}

static void my_buffer_set_empty(my_buffer_t *buf) {
    INIT_LIST_HEAD(&buf->entry);
    buf->offs = 0;
    buf->len = 0;
}

static int my_buffer_available(my_buffer_t *buf) {
    return sizeof(buf->buf) - buf->offs - buf->len;
}

static void my_init_stream(my_fcgi_stream_t *s, int for_write) {
    my_buffer_t *buf = malloc(sizeof(my_buffer_t));
    s->ev = NULL;
    s->buf = buf;
    INIT_LIST_HEAD(&buf->entry);
    buf->offs = 0;
    buf->len = 0;
    buf->alloced = 1;
    buf->for_write = for_write;
    s->active = 1;
    INIT_LIST_HEAD(&s->queue);
    s->for_write = for_write;
}

static int my_write_stream(my_fcgi_stream_t *s, char *b, int sz) {
    int to_write;

    if (s->buf->offs) {
        if (s->buf->len) {
            memmove(s->buf->buf, s->buf->buf + s->buf->offs, s->buf->len);
        }
        s->buf->offs = 0;
    }

    to_write = min_t(int, sz, sizeof(s->buf->buf) - s->buf->len);
    memcpy(s->buf->buf + s->buf->len, b, to_write);
    s->buf->len += to_write;

    return to_write;
}

static int my_stream_has_pending(my_fcgi_stream_t *s) {
    return !list_empty(&s->queue);
}

static void my_queue_push(struct list_head *head, struct list_head *entry) {
    list_add_tail(entry, head);
}

static struct list_head *my_queue_pop(struct list_head *queue) {
    struct list_head *entry;
    if (list_empty(queue))
        return NULL;
    entry = queue->next;
    list_del(queue->next);
    return entry;
}

static int my_worker_flush_stream(my_worker_request_t *req, 
        my_fcgi_stream_t *s, int notify_conn_write);

static int my_write_queue(my_worker_request_t *req, 
    my_fcgi_stream_t *s, char *p, int len) 
{
    int sz, written = 0;

    my_log_debug("my_write_queue(%d)", len);

    while (len > 0) {
        sz = my_write_stream(s, p + written, len);

        len -= sz;
        written += sz;

        if (my_buffer_is_full(s->buf)) {

            if (my_worker_flush_stream(req, s, 0)) {
                goto end;
            }
            
        } 
    }

end:
    my_log_debug("my_write_queue returned %d", written);
    return written;
}

static int my_worker_flush_stream(my_worker_request_t *req, 
        my_fcgi_stream_t *s,  int notify_conn_write) 
{
    int rc;
    my_buffer_t *buf;
    my_buffer_pool_t *pool;

    my_log_debug("my_worker_flush_stream");

    if (!s->buf->len) {
        return 0;
    }

    pool = s->for_write ? &req->conn->write_pool : &req->conn->read_pool;

    rc = 0;
    buf = my_get_buffer(pool);

    if (!buf) {
        rc = 1;
        goto notify;
    }

    my_log_debug("my_worker_flush_stream enqueued buf with %d", s->buf->len);

    buf->alloced = s->buf->alloced;
    buf->for_write = s->buf->for_write;

    swap(s->buf, buf);

    my_buffer_set_empty(s->buf);

    my_queue_push(&s->queue, &buf->entry);

    /* activate */
    if (!s->active) {
        s->active = 1;
        event_add(s->ev, NULL);
    }

notify:
    if (notify_conn_write && !req->conn->write_event_active) {
        req->conn->write_event_active = 1;
        event_add(req->conn->write_event, &req->conn->rw_timeout);
        my_log_debug("activating conn-write-event");
    }

    return rc;
}

int my_master_state_init(my_master_state_t *st) {
    int i;

    st->listen_fd = -1;

    if (!(st->workers_obj = malloc(st->workers * sizeof(my_worker_t)))) {
        my_log_crit("malloc() failed");
        return 1;
    }

    for (i = 0; i < st->workers; ++i) {
        st->workers_obj[i].pid = -1;
    }

    return 0;
}

void my_master_state_deinit(my_master_state_t *st) {
    free(st->workers_obj);
}

int my_start_listening(my_master_state_t *st) {
    int fd;
    if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        my_log_error("socket() failed %s", strerror(errno));
        return 1;
    }

    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = PF_INET;
    addr.sin_port = htons(st->listen_port);
    addr.sin_addr.s_addr = 0;

    const int val = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val))) {
        my_log_error("setsockopt() failed %s", strerror(errno));
        return 1;
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr))) { 
        my_log_error("bind() failed %s", strerror(errno));
        return 1; 
    }

    if (listen(fd, 127) < 0) {
        my_log_error("listen() failed %s", strerror(errno));
        return 1;
    }

    st->listen_fd = fd;

    return 0;
}



static int my_worker_close_connection(my_worker_connection_t *conn);
static int my_worker_abort_connection(my_worker_connection_t *conn);

static int my_worker_close_request(my_worker_request_t *req, int wait);
static int my_worker_abort_request(my_worker_request_t *req);

static void my_enqueue_to_waiting_queue_for_write(my_worker_request_t *req) {
    my_queue_push(&req->conn->waiting_for_freebuf_for_write, &req->entry);
}

static void my_worker_fcgi_handle_out_cycle(my_worker_request_t *req,
    evutil_socket_t fd, my_fcgi_stream_t *str) 
{
    int rc, cnt;

    my_log_debug("my_worker_fcgi_handle_out_cycle()");

    while (1) {
        cnt = my_buffer_available(str->buf);
        if (cnt == 0) {
            if (my_worker_flush_stream(req, str, 1)) {
                str->active = 0;
                my_enqueue_to_waiting_queue_for_write(req);
                return;
            }

            cnt = my_buffer_available(str->buf);
            assert(cnt > 0);
        }

        my_log_debug("read stdout-queue-cnt(%d)", cnt);
    
        while (cnt > 0) {
        again:
            rc = read(fd, str->buf->buf + str->buf->len, cnt);
            if (rc == -1) {
                if (errno == EINTR)
                    goto again;
    
                if (errno == EAGAIN)
                    goto out;
        
                my_log_error("read() returned %d with %s", rc, strerror(errno));
                my_worker_abort_request(req);
                return;
            }

            my_log_debug("read %d", rc);

            if (rc == 0) {
                str->active = 0;
                my_worker_flush_stream(req, str, 1);
                return;
            }
    
            str->buf->len += rc;
            cnt -= rc;
        }
    }

    return;

out:
    my_worker_flush_stream(req, str, 1);
    event_add(str->ev, NULL);
}

static void my_worker_fcgi_stderr_cycle(evutil_socket_t fd, short flags, void *arg)
{
    my_worker_request_t *req = (my_worker_request_t *)arg;
    my_worker_fcgi_handle_out_cycle(req, fd, &req->fcgi_stderr_stream);
}


static int my_worker_enqueue_stdin_end_marker(my_worker_request_t *req);

static int my_worker_enqueue_stdin_end_marker_cb(my_worker_request_t *req) {
    int rc;

    rc = my_worker_enqueue_stdin_end_marker(req);

    assert(!rc);

    return 0;
}

static void my_worker_fcgi_stdout_cycle(evutil_socket_t fd, short flags, void *arg)
{
    my_worker_request_t *req = (my_worker_request_t *)arg;
    my_worker_fcgi_handle_out_cycle(req, fd, &req->fcgi_stdout_stream);
}

static void my_worker_process_cycle(evutil_socket_t fd, short flags, void *arg);
static void my_worker_fcgi_stdin_cycle(evutil_socket_t fd, short flags, void *arg);

static void my_worker_free_buffer(my_worker_connection_t *conn, 
        my_buffer_t *b) 
{
    struct list_head *entry;
    my_callback_t *cb;
    my_worker_request_t *req;
    struct list_head *cbs_queue, *wqueue;
    my_buffer_pool_t *pool;

    if (!b->alloced) {
        return;
    }

    my_log_debug("added %p buffer to freelist", &b->entry);

    pool = b->for_write ? &conn->write_pool : &conn->read_pool;
    my_put_buffer(b, pool);

    cbs_queue = b->for_write ? &conn->waiting_for_freebuf_cbs_for_write
        : &conn->waiting_for_freebuf_cbs_for_read;

    if (!list_empty(cbs_queue)) {
        entry = my_queue_pop(cbs_queue);
        cb = list_entry(entry, my_callback_t, entry);
        cb->func(cb->arg);
        free(cb);
        return;
    }

    wqueue = b->for_write ? &conn->waiting_for_freebuf_for_write
        : &conn->waiting_for_freebuf_for_read;

    if (!list_empty(wqueue)) {
        entry = my_queue_pop(wqueue); 
        req = list_entry(entry, my_worker_request_t, entry);

        if (!b->for_write && !conn->read_event_active) {
            conn->read_event_active = 1;
            my_worker_process_cycle(conn->fd, EV_READ, conn);
            return;
        }

        /*
        if (b->for_write && !conn->write_event_active) {
            conn->write_event_active = 1;
            my_worker_process_cycle(conn->fd, EV_WRITE, conn);
            return;
        }
        */

        if (b->for_write && !req->fcgi_stdout_stream.active) {
            req->fcgi_stdout_stream.active = 1;
            my_worker_fcgi_stdout_cycle(req->fcgi_stdout[0], EV_READ, req);
            return;
        }

        if (b->for_write && !req->fcgi_stderr_stream.active) {
            req->fcgi_stderr_stream.active = 1;
            my_worker_fcgi_stderr_cycle(req->fcgi_stderr[0], EV_READ, req);
            return;
        }
    }
}

static void my_worker_fcgi_stdin_cycle(evutil_socket_t fd, short flags, void *arg)
{
    int rc;
    my_buffer_t *b, *tmp;
    my_worker_request_t *req = (my_worker_request_t *)arg;
    my_fcgi_stream_t *str;

    my_log_debug("my_worker_fcgi_stdin_cycle()");

    str = &req->fcgi_stdin_stream;

    /* turn off inactive */
    if (list_empty(&str->queue)) {
        req->fcgi_stdin_stream.active = 0;
        return;
    }

    list_for_each_entry_safe(b, tmp, &str->queue, entry) {

        my_log_debug("fcgi writing buffer %s with %d", 
            b->offs == -1 ? "stub" : "", b->len);

        if (b->offs == -1) {
            str->active = 0;

            my_log_debug("received end-marker, closing pipe");

            close(fd);

            my_log_debug("my_worker_fcgi_stdin_cycle()");
            list_del(&b->entry);
            my_worker_free_buffer(req->conn, b);
            return;
        }

        while (b->len > 0) {
            rc = write(fd, b->buf + b->offs, b->len);

            if (rc == -1) {
                if (errno == EINTR)
                    continue;

                if (errno == EAGAIN)
                    goto out;

                my_worker_abort_request(req);
                goto out;
            }

            b->len -= rc;
            b->offs += rc;
        }

        my_log_debug("my_worker_fcgi_stdin_cycle()");
        list_del(&b->entry);
        my_worker_free_buffer(req->conn, b);
    }

out:
    str->active = 1;
    event_add(str->ev, NULL);
}





static void my_worker_parse_header(my_worker_connection_t *conn) {
    char *p = conn->pending;
    fcgi_header_t *header = &conn->pending_header;
    header->version = get1(p);
    header->type = get1(p);
    header->request_id = get2(p);
    header->content_length = get2(p);
    header->padding_length = get1(p);
}

static int my_worker_init_request(my_worker_connection_t *conn) {
    int sz;
    fcgi_header_t *hdr;
    my_worker_request_t *req;

    hdr = &conn->pending_header;

    req = malloc(sizeof(my_worker_request_t));

    if (!req)
        goto no_memory;

    INIT_LIST_HEAD(&req->entry);
    req->conn = conn;
    req->id = hdr->request_id;
    req->state = req_reading_header;
    req->to_read = hdr->content_length + hdr->padding_length;
    req->to_read_header = sizeof(FCGI_BeginRequestBody);

    sz = req->to_read;
    if (!sz)
        sz = 10;

    req->pending = malloc(sz);
    req->pending_ptr = req->pending;
    req->keep_conn = 0;
    req->fcgi_pid = -1;

    my_init_stream(&req->fcgi_stdin_stream, 0);
    my_init_stream(&req->fcgi_stdout_stream, 1);
    my_init_stream(&req->fcgi_stderr_stream, 1);

    INIT_LIST_HEAD(&req->packet_batch);
    INIT_LIST_HEAD(&req->env);
    INIT_LIST_HEAD(&req->terminate_callbacks);

    if (!req->pending)
        goto no_memory;

    my_log_debug("inited request with %p", req->pending);

    conn->reqs[hdr->request_id] = req;
    conn->state = conn_reading_body;
    return 0;

no_memory:
    my_log_error("malloc() failed");
    return 1;
}

static int my_worker_create_fcgi_application(my_worker_connection_t *conn, 
        my_worker_request_t *req) 
{
    int pid, env_elems, env_len;
    char *argv[2];
    char **p, **envp, *env_mem;
    key_value_t *kv;
    struct event *ev_stdin, *ev_stdout, *ev_stderr;
    my_worker_state_t *worker_state = conn->worker_state;
    my_master_state_t *master_state = worker_state->master_state;

    if (pipe(req->fcgi_stdin) == -1
        || pipe(req->fcgi_stdout) == -1
        || pipe(req->fcgi_stderr) == -1)
    {
        my_log_error("fcgi-pipe failed() with %s", strerror(errno));
        return 1;
    }

    if ((pid = fork()) == -1) {
        my_log_error("fcgi-fork failed() with %s", strerror(errno));
        return 1;
    }

    if (pid) {
        my_log_debug("created fastcgi process with pid=%d", pid);
    }

    /* child */
    if (!pid) {
        close(req->fcgi_stdin[1]);
        close(req->fcgi_stdout[0]);
        close(req->fcgi_stderr[0]);

        if (dup2(req->fcgi_stdin[0], 0) == -1
            || dup2(req->fcgi_stdout[1], 1) == -1
            || dup2(req->fcgi_stderr[1], 2) == -1
            || close(req->fcgi_stdin[0])
            || close(req->fcgi_stdout[1])
            || close(req->fcgi_stderr[1]))
        {
            goto err;
        }

        argv[0] = (char*)master_state->fastcgi_path;
        argv[1] = NULL;

        env_elems = 1;
        list_for_each_entry(kv, &req->env, entry) {
            ++env_elems;
            env_len += kv->key_length + 1 + kv->value_length + 1;
        }

        if (!(envp = malloc((sizeof(void*) * env_elems)))
            || !(env_mem = malloc(env_len)))
        {
            goto err;
        }

        p = envp;
        list_for_each_entry(kv, &req->env, entry) {
            *p = &env_mem[0];
            memcpy(env_mem, kv->key, kv->key_length);
            env_mem += kv->key_length;
            *env_mem++ = '=';
            memcpy(env_mem, kv->value, kv->value_length);
            env_mem += kv->value_length;
            *env_mem++ = 0;
            ++p;
        }

        *p = NULL;

        execve(argv[0], argv, envp);
    err:
        my_log_error("fcgi-application failed with %s", strerror(errno));
        exit(1);
    }

    req->fcgi_pid = pid;
    close(req->fcgi_stdin[0]);
    close(req->fcgi_stdout[1]);
    close(req->fcgi_stderr[1]);

    fcntl(req->fcgi_stdin[1], F_SETFD, FD_CLOEXEC);
    fcntl(req->fcgi_stdout[0], F_SETFD, FD_CLOEXEC);
    fcntl(req->fcgi_stderr[0], F_SETFD, FD_CLOEXEC);

    fcntl(req->fcgi_stdin[1], F_SETFL, 
            fcntl(req->fcgi_stdin[1], F_GETFL, 0) | O_NONBLOCK);
    fcntl(req->fcgi_stdout[0], F_SETFL, 
            fcntl(req->fcgi_stdout[0], F_GETFL, 0) | O_NONBLOCK);
    fcntl(req->fcgi_stderr[0], F_SETFL, 
            fcntl(req->fcgi_stderr[0], F_GETFL, 0) | O_NONBLOCK);

    ev_stdin = event_new(worker_state->evbase, req->fcgi_stdin[1], EV_WRITE,
        &my_worker_fcgi_stdin_cycle, req);

    ev_stdout = event_new(worker_state->evbase, req->fcgi_stdout[0], EV_READ,
        &my_worker_fcgi_stdout_cycle, req);

    ev_stderr = event_new(worker_state->evbase, req->fcgi_stderr[0], EV_READ,
        &my_worker_fcgi_stderr_cycle, req);

    if (!ev_stdin || !ev_stdout || !ev_stderr
        || event_add(ev_stdin, NULL) == -1
        || event_add(ev_stdout, NULL) == -1
        || event_add(ev_stderr, NULL) == -1)
    {
        my_log_error("event_creation error()");
        return 1;
    }

    req->fcgi_stdin_stream.ev = ev_stdin;
    req->fcgi_stdout_stream.ev = ev_stdout;
    req->fcgi_stderr_stream.ev = ev_stderr;

    return 0;
}

static int my_worker_subinit_request(my_worker_connection_t *conn) {
    int sz;
    my_worker_request_t *req;
    fcgi_header_t *hdr = &conn->pending_header;

    req = conn->reqs[conn->pending_header.request_id];

    my_log_debug("subinit request with %p", req->pending);

    sz = req->to_read = hdr->content_length + hdr->padding_length;

    if (!sz)
        sz = 10;

    if (!(req->pending = realloc(req->pending, sz))) {
        my_log_error("realloc() failed");
        return 1;
    }

    /* is time to create fastcgi app */
    if (req->fcgi_pid == -1
        && hdr->type == FCGI_STDIN
        && my_worker_create_fcgi_application(conn, req))
    {
        return 1;
    }

    req->pending_ptr = req->pending;
    if (req->to_read) {
        req->state = req_reading_body;
        conn->state = conn_reading_body;
    } else {
        req->state = req_reading_header;
        conn->state = conn_reading_header;
    }
    return 0;
}

static int my_parse_key_values(my_worker_connection_t *conn, 
    my_worker_request_t *req, fcgi_header_t *hdr)
{
    char *p;
    key_value_t *kv;
    int key_length, value_length;

    p = req->pending;
    while (p < req->pending + hdr->content_length) {
        if ((key_length = get1(p)) & 0x80) {
            key_length &= 0x7f;
            key_length <<= 24;
            key_length |= get3(p);
        }

        if ((value_length = get1(p)) & 0x80) {
            value_length &= 0x7f;
            value_length <<= 24;
            value_length |= get3(p);
        }

        if (!(kv = malloc(sizeof(key_value_t)))) {
            my_log_error("malloc() failed");
            return 1;
        }

        INIT_LIST_HEAD(&kv->entry);

        kv->key = malloc(key_length);
        kv->key_length = key_length;

        if (!kv->key)
            return 1;

        memcpy(kv->key, p, key_length);
        p += key_length;

        kv->value = malloc(value_length);
        kv->value_length = value_length;

        if (!kv->value)
            return 1;

        memcpy(kv->value, p, value_length);
        p += value_length;

        list_add(&kv->entry, &req->env);
    }

    return 0;
}

static void my_enqueue_to_waiting_queue_for_read(my_worker_request_t *req) {
    my_queue_push(&req->conn->waiting_for_freebuf_for_read, &req->entry);
}



static int my_worker_process_request(my_worker_connection_t *conn, 
    my_worker_request_t *req) 
{
    char **p;
    int role, flags, rc, len;
    fcgi_header_t *hdr;
    my_fcgi_stream_t *str;

    my_log_debug("my_worker_process_request");

    /* begin is ready */
    if (req->state == req_reading_header) {
        p = &req->pending_ptr;
        role = get2(*p);
        flags = get1(*p);

        if (role != FCGI_RESPONDER) {
            my_log_error("role %d is not supported", role);
            return 1;
        }

        req->keep_conn = flags & FCGI_KEEP_CONN ? 1 : 0;

        conn->pending_ptr = conn->pending;
        conn->state = conn_reading_header;

    } else {
        hdr = &conn->pending_header;

        assert(hdr->type == FCGI_PARAMS || hdr->type == FCGI_STDIN);

        if (hdr->type == FCGI_PARAMS) {
            if (my_parse_key_values(conn, req, hdr))
                return 1;

            my_log_debug("parsed key-values successfully");
        }

        if (hdr->type == FCGI_STDIN) {

            str = &req->fcgi_stdin_stream;

            p = &req->pending_ptr;
            len = conn->pending_header.content_length - (*p - req->pending);

            while (len > 0) {
                rc = my_write_queue(req, str, *p, len);

                *p += rc;

                if (rc != len) {
                    len -= rc;
                    goto waiting_for_buffer;
                }

                len -= rc;
            }

            if (my_worker_flush_stream(req, str, 0)) {
        waiting_for_buffer:
                my_log_debug("deactivating connection-read");

                conn->state = conn_processing_request;
                conn->read_event_active = 0;
                my_enqueue_to_waiting_queue_for_read(req);
                return 2;
            }
        }

        conn->pending_ptr = conn->pending;
        conn->state = conn_reading_header;
    }

    req->state = req_reading_header;

    return 0;
}

static int my_worker_check_request_id(my_worker_connection_t *conn,
    int exists_check, int closing_check)
{
    int reqid = conn->pending_header.request_id;

    if (reqid <= FCGI_NULL_REQUEST_ID  || reqid >= 65536) {
        my_log_error("invalid reqid %d", reqid);
        return 1;
    }

    if (exists_check && !conn->reqs[reqid]) {
        my_log_error("no request with reqid %d", reqid);
        return 1;
    }

    if (closing_check && conn->reqs[reqid]->state == req_closing) {
        return 1;
    }

    return 0;
}

static int my_construct_fcgi_header(char *b, int type, int rid, int len);

static int my_worker_enqueue_end_request(my_worker_request_t *req);

static int my_worker_enqueue_stdout_eof_cb(void *arg) {
    my_buffer_t *buf;
    my_worker_request_t *req = (my_worker_request_t *)arg;

    if (!(buf = my_get_buffer(&req->conn->write_pool)))
        return 1;

    buf->len = my_construct_fcgi_header(buf->buf, 
            FCGI_STDOUT, req->id, 0);

    my_log_debug("enqueued end-request-stdout start");

    my_queue_push(&req->conn->output_queue, &buf->entry);

    if (!req->conn->write_event_active)
        event_add(req->conn->write_event, NULL);

    my_log_debug("enqueued end-request-stdout done");

    return 0;
}

static int my_worker_enqueue_stderr_eof_cb(void *arg) {
    my_buffer_t *buf;
    my_worker_request_t *req = (my_worker_request_t *)arg;

    if (!(buf = my_get_buffer(&req->conn->write_pool)))
        return 1;

    buf->len = my_construct_fcgi_header(buf->buf, 
            FCGI_STDERR, req->id, 0);

    my_log_debug("enqueued end-request-stdout start");

    my_queue_push(&req->conn->output_queue, &buf->entry);

    if (!req->conn->write_event_active)
        event_add(req->conn->write_event, NULL);

    my_log_debug("enqueued end-request-stdout done");

    return 0;
}

static int my_worker_enqueue_end_request_cb(void *arg) {
    int rc;

    rc = my_worker_enqueue_end_request(arg);

    assert(!rc);

    return 0;
}

static int my_worker_request_terminate_cb(void *arg);

static int my_worker_close_request(my_worker_request_t *req, int wait) {
    my_callback_t *cb;
    my_buffer_t *buf;

    req->state = req_closing;

    if (req->fcgi_pid != -1) {

        if (!wait) {

            my_log_debug("killing fcgi-application with pid %d", req->fcgi_pid);

            if (kill(req->fcgi_pid, SIGKILL) != -1) 
                waitpid(req->fcgi_pid, 0);

            req->fcgi_pid = -1;

        } else {

            my_log_debug("enqueing stdin end-marker");

            if (my_worker_enqueue_stdin_end_marker(req)) {
                cb = my_get_callback(
                    (int(*)(void*))&my_worker_enqueue_stdin_end_marker_cb, req);

                if (!cb)
                    goto out_of_memory;

                my_queue_push(&req->conn->waiting_for_freebuf_cbs_for_read, 
                        &cb->entry);
            }

            return 0;
        }

    }

    my_log_debug("enqueing end request");

    cb = my_get_callback(&my_worker_enqueue_stdout_eof_cb, req);
    my_queue_push(&req->terminate_callbacks, &cb->entry);

    cb = my_get_callback(&my_worker_enqueue_stderr_eof_cb, req);
    my_queue_push(&req->terminate_callbacks, &cb->entry);

    cb = my_get_callback(&my_worker_enqueue_end_request_cb, req);
    my_queue_push(&req->terminate_callbacks, &cb->entry);
    my_worker_request_terminate_cb(req);

    return 0;

out_of_memory:
    my_log_crit("malloc() failed");
    return 1;
}

void my_worker_deinit_stream(my_worker_connection_t *conn, my_fcgi_stream_t *str) {
    my_buffer_t *b, *tmp;

    if (str->ev) {
        event_del(str->ev);
    }

    list_for_each_entry_safe(b, tmp, &str->queue, entry) {
        list_del(&b->entry);
        my_put_buffer(b, b->for_write ? &conn->write_pool : &conn->read_pool);
    }

    free(str->buf);
}

void my_worker_free_kv(struct list_head *kv) {
    key_value_t *p, *tmp;
    list_for_each_entry_safe(p, tmp, kv, entry) {
        free(p->key);
        free(p->value);
    }
}

static void my_worker_free_request(my_worker_request_t *req) {
    my_worker_connection_t *conn;

    my_log_debug("my_worker_free_request() begin");

    conn = req->conn;
    conn->reqs[req->id] = NULL;

    if (req->pending) {
        free(req->pending);
        req->pending = NULL;
    }

    if (req->fcgi_pid != -1) {
        req->fcgi_pid = -1;

        close(req->fcgi_stdin[1]);
        close(req->fcgi_stdout[0]);
        close(req->fcgi_stderr[0]);
    }

    my_worker_deinit_stream(req->conn, &req->fcgi_stdin_stream);
    my_worker_deinit_stream(req->conn, &req->fcgi_stdout_stream);
    my_worker_deinit_stream(req->conn, &req->fcgi_stderr_stream);

    my_log_debug("freeing kvs");
    my_worker_free_kv(&req->env);

    conn->request_count -= 1;

    my_log_debug("my_worker_free_request() done");
}

static int my_worker_abort_request(my_worker_request_t *req) {

    my_log_info("aborting request with conn-fd=%d", req->conn->fd);

    if (req->fcgi_pid != -1) {

        my_log_debug("killing fcgi-application with pid %d", req->fcgi_pid);

        if (kill(req->fcgi_pid, SIGKILL) != -1) 
            waitpid(req->fcgi_pid, 0);

        req->fcgi_pid = -1;
    }

    my_worker_free_request(req);

    return 0;
}

static int my_worker_free_connection(my_worker_connection_t *conn) {
    my_buffer_t *b, *tmp;

    event_del(conn->read_event);

    event_del(conn->write_event);

    my_buffer_pool_destroy(&conn->read_pool);
    my_buffer_pool_destroy(&conn->write_pool);

    list_del(&conn->entry);

    free(conn);
}

static int my_worker_abort_connection(my_worker_connection_t *conn) {
    int i;
    my_buffer_t *b, *tmp;
    my_worker_request_t *req;

    my_log_info("aborting connection with fd=%d", conn->fd);

    for (i = 0; i < 65536; ++i) {
        req = conn->reqs[i];

        if (req)
            my_worker_abort_request(req);
    }

    close(conn->fd);

    list_for_each_entry_safe(b, tmp, &conn->output_queue, entry) {
        list_del(&b->entry);
        my_put_buffer(b, b->for_write ? &conn->write_pool : &conn->read_pool);
    }

    my_worker_free_connection(conn);

    return 0;
}

static int my_worker_close_connection(my_worker_connection_t *conn) {
    int i;
    my_worker_request_t *req;

    my_log_info("closing connection with fd=%d", conn->fd);

    for (i = 0; i < 65536; ++i) {
        req = conn->reqs[i];

        if (req)
            my_worker_close_request(req, 1);
    }

    return 0;
}

static int my_worker_read_process_cycle(my_worker_connection_t *conn, int fd) 
{
    int rc, to_read;
    char scratch[4096];
    fcgi_header_t *hdr;
    my_worker_request_t *req;

    my_log_debug("my_worker_read_process_cycle (%d/%d)", 
            conn->state, conn->pending_header.request_id);

    while (1) {
        switch (conn->state) {
        case conn_reading_header: {
            to_read = FCGI_HEADER_LEN - (conn->pending_ptr - conn->pending);

            again: rc = read(fd, conn->pending_ptr, to_read);

            if (rc <= 0) {
                if (rc < 0 && errno == EINTR)
                    goto again;
                if (rc < 0 && errno == EAGAIN)
                    return 0;

                my_log_error("read() returned %d with %s", rc, strerror(errno));
                goto err;

            } else if (rc != to_read) {
                /* need more data */
                conn->pending_ptr += rc;
                return 0;
            }

            my_worker_parse_header(conn);

            if (conn->pending_header.version != FCGI_VERSION_1) {
                my_log_error("request arrived with invalid version %d",
                    conn->pending_header.version);
                goto err;
            }

            hdr = &conn->pending_header;

            my_log_debug("read header %d/%d/%d/%d/%d", 
                    hdr->version, hdr->type, hdr->request_id, 
                    hdr->content_length, hdr->padding_length);

            switch (hdr->type) {
            case FCGI_BEGIN_REQUEST:
                if (my_worker_check_request_id(conn, 0, 0)
                    || my_worker_init_request(conn))
                { goto err; }
                break;
            case FCGI_ABORT_REQUEST:
                if (my_worker_check_request_id(conn, 1, 0)
                    || my_worker_close_request(conn->reqs[ 
                        conn->pending_header.request_id], 0))
                { goto err; }
                break;
            case FCGI_END_REQUEST:
                if (my_worker_check_request_id(conn, 1, 0)
                    || my_worker_close_request(conn->reqs[ 
                        conn->pending_header.request_id], 1))
                { goto err; }
                break;
            case FCGI_PARAMS: case FCGI_STDIN:

                if (my_worker_check_request_id(conn, 1, 0)) {
                    conn->state = conn_ignoring_body;
                    hdr = &conn->pending_header;
                    conn->ignore_to_read = hdr->content_length 
                        + hdr->padding_length;
                    conn->ignore_read = 0;
                } else 
                if (my_worker_subinit_request(conn)) {
                    goto err;
                }

                break;
            case FCGI_STDOUT: goto unexpected;
            case FCGI_STDERR: goto unexpected;
            case FCGI_DATA: goto unsupported;
            case FCGI_GET_VALUES: goto unsupported;
            case FCGI_GET_VALUES_RESULT: goto unsupported;
            default:
                assert(hdr->type > FCGI_MAXTYPE);
                goto unexpected;
            };

            break;

unsupported:
            my_log_error("unsupported type arrived %d", hdr->type);
            goto err;

unexpected:
            my_log_error("unpexpected type arrived %d", hdr->type);
            goto err;
        }

        case conn_reading_body: {
            assert(conn->pending_header.request_id != -1);

            req = conn->reqs[conn->pending_header.request_id];

            assert(req != NULL);

            to_read = 0;
            switch (req->state) {
            case req_reading_header:
                to_read = req->to_read_header - (req->pending_ptr - req->pending);
                break;
            case req_reading_body:
                to_read = req->to_read - (req->pending_ptr - req->pending);
                break;
            default: assert(0);
            };

            again1: rc = read(fd, req->pending, to_read);

            if (rc <= 0) {
                if (rc < 0 && errno == EINTR)
                    goto again1;
                if (rc < 0 && errno == EAGAIN)
                    return 0;
                my_log_error("read() returned %d with %s", rc, strerror(errno));
                goto err;
            } else if (rc != to_read) {
                /* need more data */
                req->pending_ptr += rc;
                my_log_debug("read body %d/%d", rc, to_read);
                return 0;
            }
            
            req->pending_ptr = req->pending;

            my_log_debug("read process body");

            if ((rc = my_worker_process_request(conn, req)))
                return rc;

            break;
        }

        case conn_ignoring_body: {

            while (conn->ignore_read < conn->ignore_to_read) {

                to_read = min_t(int, conn->ignore_to_read 
                        - conn->ignore_read, 4096);

                again2: rc = read(fd, scratch, to_read);

                if (rc <= 0) {
                    if (rc < 0 && errno == EINTR)
                        goto again2;
                    if (rc < 0 && errno == EAGAIN)
                        return 0;

                    my_log_error("read() returned %d with %s", 
                            rc, strerror(errno));
                    goto err;
                } 

                conn->ignore_read += rc;
            }

            conn->pending_ptr = conn->pending;
            conn->state = conn_reading_header;
            break;
        }

        case conn_processing_request: {
            req = conn->reqs[conn->pending_header.request_id];
            if ((rc = my_worker_process_request(conn, req)))
                return rc;
            break;
        }};
    }
    return 0;

err:
    return 1;
}

static int my_worker_output_queue(my_worker_connection_t *conn,
        struct list_head *queue, int fd) 
{
    int rc, len;
    my_buffer_t *b, *tmp;

    list_for_each_entry_safe(b, tmp, queue, entry) {

        while (b->len > 0) {
        again: 
            rc = write(fd, b->buf + b->offs, b->len);
        
            if (rc == -1) {
                if (errno == EINTR) 
                    goto again;

                if (errno == EAGAIN) {
                    rc = 2;
                    goto err;
                }
        
                rc = 1;
                goto err;
            }

            b->offs += rc;
        
            if (rc != b->len) {
                b->len -= rc;

                rc = 2;
                goto err;
            }

            b->len -= rc;
        }

        my_log_debug("my_worker_output_queue()");
        list_del(&b->entry);
        my_worker_free_buffer(conn, b);
    }

    rc = 0;

err:
    if (rc) {
        my_log_error("WRITE_ERROR %d %d %s", rc, errno, strerror(errno));
    }
    return rc;
}

static int find_queue_for_write(my_worker_connection_t *conn) 
{
    int i;
    my_worker_request_t *req;

    my_log_debug("find_queue_for_write()");

    for (i = 0; i < 65536; ++i) {
        if (req = conn->reqs[i]) {

            if (my_stream_has_pending(&req->fcgi_stdout_stream)) {
                conn->output_queue_state = conn_writing_stdout_queue_begin;
                conn->active_write_reqid = i;
                return 0;
            }

            if (my_stream_has_pending(&req->fcgi_stderr_stream)) {
                conn->output_queue_state = conn_writing_stderr_queue_begin;
                conn->active_write_reqid = i;
                return 0;
            }

        }
    }

    if (!list_empty(&conn->output_queue)) {
        conn->output_queue_state = conn_writing_queue;
        return 0;
    }

    /* nothing found */
    conn->output_queue_state = conn_writing_none;
    return 1;
}

static int my_construct_fcgi_header(char *b, int type, int rid, int len) {
    b = put1(b, FCGI_VERSION_1);
    b = put1(b, type);
    b = put2(b, rid);
    b = put2(b, len);
    b = put1(b, 0);
    b = put1(b, 0);
    return FCGI_HEADER_LEN;
}

static int my_construct_fcgi_end_request_body(char *b, 
        int app_status, int proto_status) 
{
    b = put4(b, app_status);
    b = put1(b, proto_status);
    return 8;
}

static int my_construct_fcgi_end_request(char *b, int rid, int app_status) 
{
    int len;
    len = my_construct_fcgi_end_request_body(b + FCGI_HEADER_LEN, 
        app_status, FCGI_REQUEST_COMPLETE);
    len += my_construct_fcgi_header(b, FCGI_END_REQUEST, rid, len);
    return len;
}


static void my_worker_setup_output_queue(my_worker_request_t *req, 
   struct list_head *queue) 
{
    int len, cnt, cnt1;
    my_buffer_t *b, *tmp, *header; 
    
    header = &req->packet_header;
    my_buffer_set_empty(header);

    header->len = FCGI_HEADER_LEN;
    header->alloced = 0;

    assert(list_empty(&req->packet_batch));

    list_add(&header->entry, &req->packet_batch);

    cnt = len = 0;
    list_for_each_entry_safe(b, tmp, queue, entry) {
        if (len + b->len >= 65536) {
            break;
        }

        list_move_tail(&b->entry, &req->packet_batch);

        len += b->len;
        cnt++;
    }

    cnt1 = 0;
    list_for_each_entry(b, queue, entry) {
        cnt1++;
    }

    my_log_debug("constructed batch with %d/%d, have %d left", cnt, len, cnt1);

    my_construct_fcgi_header(header->buf, FCGI_STDOUT, req->id, len);
}

static int my_worker_write_process_cycle(my_worker_connection_t *conn, int fd) 
{
    int rc, len, type, reqid;
    char *p;
    my_buffer_t *b;
    my_worker_request_t *req;
    my_fcgi_stream_t *str;

    while (1) {
        my_log_debug("my_worker_write_process_cycle (%d/%d/%d/%d)", 
            conn->state, conn->pending_header.request_id, 
            conn->output_queue_state, conn->active_write_reqid);

        switch (conn->output_queue_state) {
        case conn_writing_none: {

            /* none found, temporarily deactivate */
            if (find_queue_for_write(conn))
                return 2;
    
            break;
        }

        case conn_writing_queue: {

            /* here is a safe place to free requests */

            list_for_each_entry(b, &conn->output_queue, entry) {
                p = b->buf + 1;
                type = get1(p);
                reqid = get2(p);

                if (type == FCGI_END_REQUEST && conn->reqs[reqid]) {
                    my_worker_free_request(conn->reqs[reqid]);
                }
            }

            my_log_debug("writing output-queue begin");
            rc = my_worker_output_queue(conn, &conn->output_queue, fd);

            if (rc) {
                if (rc == 2)
                    return rc;

                goto err;
            }

            /* we wrote all */
            conn->output_queue_state = conn_writing_none;
            break;
        }

        case conn_writing_stdout_queue_begin: {
            assert(conn->active_write_reqid != -1);

            req = conn->reqs[conn->active_write_reqid];
            str = &req->fcgi_stdout_stream;

            my_worker_setup_output_queue(req, &str->queue);
            conn->output_queue_state = conn_writing_stdout_queue;
            break;
        } 

        case conn_writing_stdout_queue: {
            assert(conn->active_write_reqid != -1);

            req = conn->reqs[conn->active_write_reqid];
            str = &req->fcgi_stdout_stream;

            rc = my_worker_output_queue(conn, &req->packet_batch, fd);

            if (rc) {
                if (rc == 2)
                    return rc;

                goto err;
            }

            assert(list_empty(&req->packet_batch));

            /* we wrote all */
            if (!list_empty(&str->queue)) {
                conn->output_queue_state = conn_writing_stdout_queue_begin;
            } else {
                conn->active_write_reqid = -1;
                conn->output_queue_state = conn_writing_none;
            }

            break;
        }

        case conn_writing_stderr_queue_begin: {
            assert(conn->active_write_reqid != -1);

            req = conn->reqs[conn->active_write_reqid];
            str = &req->fcgi_stderr_stream;

            my_worker_setup_output_queue(req, &str->queue);
            conn->output_queue_state = conn_writing_stderr_queue;
            break;
        }

        case conn_writing_stderr_queue: {
            assert(conn->active_write_reqid != -1);

            req = conn->reqs[conn->active_write_reqid];
            str = &req->fcgi_stderr_stream;

            rc = my_worker_output_queue(conn, &req->packet_batch, fd);

            if (rc) {
                if (rc == 2)
                    return rc;

                goto err;
            }

            assert(list_empty(&req->packet_batch));

            /* we wrote all */
            if (!list_empty(&str->queue)) {
                conn->output_queue_state = conn_writing_stderr_queue_begin;
            } else {
                conn->active_write_reqid = -1;
                conn->output_queue_state = conn_writing_none;
            }

            break;
        }

        default:
            my_log_crit("unknown output-queue state `%d'", 
                    conn->output_queue_state); 
            exit(1);
        };
    }

    my_log_debug("my_worker_write_process_cycle done");

    return 0;

err:
    return 1;
}

static void my_worker_process_cycle(evutil_socket_t fd, short flags, void *arg) {
    int to_read, rc;
    struct event *ev;
    my_worker_connection_t *conn = (my_worker_connection_t *)arg;
    my_worker_request_t *req;

    my_log_debug("my_worker_process_cycle(%x)", flags);

    if (flags & EV_TIMEOUT) {
        my_log_error("event-timeout occured()");
        goto err;
    }

    if (flags & EV_READ) {
        rc = my_worker_read_process_cycle(conn, fd);

        if (rc == 2) {
            my_log_debug("deactivating conn-read-event");
            conn->read_event_active = 0;
        } else if (rc) {
            goto err;
        } else {
            ev = conn->read_event;
    
            if (event_add(ev, &conn->rw_timeout)) {
                my_log_error("event_add() failed");
                goto err;
            }
    
            conn->read_event_active = 1;
        }
    }

    if (flags & EV_WRITE) {
        rc = my_worker_write_process_cycle(conn, fd);

        if (rc == 2) {
            my_log_debug("deactivating conn-write-event");
            conn->write_event_active = 0;

        } else if (rc) {
            goto err;
        } else {
            ev = conn->write_event;
    
            if (event_add(ev, &conn->rw_timeout)) {
                my_log_error("event_add() failed");
                goto err;
            }
    
            conn->write_event_active = 1;
        }
    }

    return;

err:
    my_worker_abort_connection(conn);
}

int my_worker_state_init(my_master_state_t *master_state, my_worker_state_t *st) {
    st->evbase = NULL;
    st->evlisten = NULL;
    st->master_state = master_state;
    INIT_LIST_HEAD(&st->conns);
    return 0;
}

int my_worker_connection_init(my_worker_state_t *st, int sockfd) {
    struct event *ev;
    my_worker_connection_t *conn;

    conn = malloc(sizeof(my_worker_connection_t));

    if (!conn) {
        my_log_error("malloc() failed() %s", strerror(errno));
        return 1;
    }

    memset(conn, 0, sizeof(*conn));
    INIT_LIST_HEAD(&conn->entry);
    conn->fd = sockfd;
    conn->state = conn_reading_header;
    conn->request_count = 0;
    conn->pending_ptr = conn->pending;
    conn->worker_state = st;
    conn->ignore_to_read = 0;
    conn->ignore_read = 0;
    conn->rw_timeout.tv_sec = 20;
    conn->rw_timeout.tv_usec = 0;

    my_buffer_pool_init(&conn->read_pool, 30, 0);
    my_buffer_pool_init(&conn->write_pool, 30, 1);

    conn->read_event = NULL;
    conn->write_event = NULL;

    ev = event_new(st->evbase, sockfd,
        EV_READ, &my_worker_process_cycle, (void *)conn);
    if (!ev || event_add(ev, &conn->rw_timeout) == -1) {
        my_log_error("event_new/add() failed()");
        return 1;
    }

    conn->read_event_active = 1;
    conn->read_event = ev;

    ev = event_new(st->evbase, sockfd,
        EV_WRITE, &my_worker_process_cycle, (void *)conn);
    if (!ev || event_add(ev, &conn->rw_timeout) == -1) {
        my_log_error("event_new/add() failed()");
        return 1;
    }

    conn->write_event_active = 1;
    conn->write_event = ev;

    conn->output_queue_state = conn_writing_none;
    INIT_LIST_HEAD(&conn->output_queue);
    conn->active_write_reqid = -1;

    INIT_LIST_HEAD(&conn->waiting_for_freebuf_cbs_for_read);
    INIT_LIST_HEAD(&conn->waiting_for_freebuf_cbs_for_write);

    INIT_LIST_HEAD(&conn->waiting_for_freebuf_for_read);
    INIT_LIST_HEAD(&conn->waiting_for_freebuf_for_write);

    list_add(&conn->entry, &st->conns);

    return 0;
}


void my_worker_listen_cycle(evutil_socket_t fd, short flags, void *arg);

int my_init_worker_events(my_worker_state_t *wst, my_master_state_t *mst) {
    wst->evbase = event_base_new();
    wst->evlisten = event_new(wst->evbase, mst->listen_fd, 
        EV_READ, &my_worker_listen_cycle, wst);
    if (!wst->evbase 
        || !wst->evlisten 
        || event_add(wst->evlisten, NULL) == -1)
    {
        my_log_error("worker-event creation error");
        return 1;
    }
    return 0;
}

void my_stop_workers(my_master_state_t *st) {
    int i, pid;
    for (i = 0; i < st->workers; ++i) {
        if ((pid = st->workers_obj[i].pid) != -1) {
            my_log_info("stopping process %d", pid);
            kill(pid, SIGTERM);
        }
    }
}

void my_kill_workers(my_master_state_t *st) {
    int i, pid;
    for (i = 0; i < st->workers; ++i) {
        if ((pid = st->workers_obj[i].pid) != -1) {
            my_log_info("killing process %d", pid);
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);

            st->workers_obj[i].pid = -1;
        }
    }
}

int my_count_alive_workers(my_master_state_t *st) {
    int i, cnt = 0;
    for (i = 0; i < st->workers; ++i) {
        if (st->workers_obj[i].pid != -1) {
            cnt++;
        }
    }
    return cnt;
}

static my_worker_t *my_find_worker_by_pid(my_master_state_t *st, int pid);


static int my_master_should_stop = 0;
static int my_master_should_handle_children = 0;

void master_sighandler_term(int c) {
    //my_log_debug("signal %d was delivered", c);
    my_master_should_stop = 1;
}
void master_sighandler_child(int c) {
    //my_log_debug("signal %d was delivered", c);
    my_master_should_handle_children = 1;
}
void my_init_master_signals() {
    signal(SIGTERM, &master_sighandler_term);
    signal(SIGINT, &master_sighandler_term);
    signal(SIGALRM, &master_sighandler_term);
    signal(SIGCHLD, &master_sighandler_child);
}

int my_wait_for_workers(my_master_state_t *st, 
        int with_terminate, int with_restart) 
{
    int rc;
    my_worker_t *w;
    struct timeval tv_limit, tvnow;
    sigset_t signals_to_wait, empty_sigset;
    struct timeval loop_time = { 2, 0 }; /* 2s */
    //struct timespec wait_time = { 0, 10 * 1000 * 1000 }; /* 10ms */

    sigemptyset(&signals_to_wait);
    sigaddset(&signals_to_wait, SIGINT);
    sigaddset(&signals_to_wait, SIGTERM);
    sigaddset(&signals_to_wait, SIGCHLD);
    sigaddset(&signals_to_wait, SIGALRM);

    sigemptyset(&empty_sigset);

    sigprocmask(SIG_BLOCK, &signals_to_wait, NULL);

    gettimeofday(&tv_limit, NULL);
    time_add(&tv_limit, &loop_time);

    if (!with_terminate) {
        alarm(loop_time.tv_sec);
    }

    while (1) {

        sigsuspend(&empty_sigset);

        if (with_terminate) {

            if (my_master_should_stop)
                break;

        } else {
            gettimeofday(&tvnow, NULL);

            if (time_after(&tvnow, &tv_limit)) {
                break;
            }
        }

        if (my_master_should_handle_children) {
            my_master_should_handle_children = 0;

            while (1) {
                rc = waitpid(-1, NULL, WNOHANG);

                if (rc <= 0)
                    break;

                if (with_restart) {
                    if (my_restart_worker(st, rc))
                        return 3;
                } else {
                    w = my_find_worker_by_pid(st, rc);
                    w->pid = -1;
                }
            }
        }

        /* we are done */
        if (!my_count_alive_workers(st)) {
            break;
        }
    }

    sigprocmask(SIG_UNBLOCK, &signals_to_wait, NULL);
}

int my_start_worker(my_master_state_t *st) {
    int pid, rc;

    if ((pid = fork()) == -1) {
        return 0;
    }

    if (!pid) {
        my_log_info("worker process %d started", getpid());
        rc = st->worker_cycle_proc(st);
        exit(rc);
    }

    return pid;
}

int my_start_workers(my_master_state_t *st) {
    int i, pid, rc;
    for (i = 0; i < st->workers; ++i) {

        if (pid = my_start_worker(st)) {
            st->workers_obj[i].pid = pid;
        } else {
            my_log_error("fork() failed %s. stopping workers", strerror(errno));
            my_stop_workers(st);
            return 1;
        }
    }

    return 0;
}

static my_worker_t *my_find_worker_by_pid(my_master_state_t *st, int pid) {
    int i;
    for (i = 0; i < st->workers; ++i) {
        if (st->workers_obj[i].pid == pid) {
            return &st->workers_obj[i];
        }
    }
    return NULL;
}

int my_restart_worker(my_master_state_t *st, int pid) {
    my_worker_t *w;

    w = my_find_worker_by_pid(st, pid);
    if (!w)
        return 1;

    if (pid = my_start_worker(st)) {
        w->pid = pid;
        return 0;
    } else {
        my_log_error("fork() failed %s. stopping workers", 
                strerror(errno));
        my_stop_workers(st);
        return 1;
    }
}

static int worker_should_stop = 0;
static int worker_should_handle_children = 0;
void worker_sighandler_term(int c) {
    //my_log_debug("signal %d to child %d was delivered", c, getpid());
    worker_should_stop = 1;
}
void worker_sighandler_child(int c) {
    //my_log_debug("signal %d to child %d was delivered", c, getpid());
    worker_should_handle_children = 1;
}

void my_init_worker_signals() {
    signal(SIGTERM, &worker_sighandler_term);
    signal(SIGINT, SIG_IGN);
    signal(SIGCHLD, &worker_sighandler_child);
}

void my_worker_listen_cycle(evutil_socket_t fd, short flags, void *arg) {
    int sockfd;
    my_worker_state_t *st = (my_worker_state_t *)arg;

    if ((sockfd = accept4(fd, NULL, NULL, SOCK_NONBLOCK)) == -1) {
        my_log_error("accept failed() %s", strerror(errno));
        goto err;
    }

    my_log_info("got connection with fd=%d", sockfd);

    if (my_worker_connection_init(st, sockfd)) {
        goto err;
    }

    if (event_add(st->evlisten, NULL) == -1) {
        my_log_error("event_add() failed");
        goto err;
    }

    return;

err:
    my_log_error("listen_cycle(): should_stop =1");
    worker_should_stop = 1;
    return;
}

static my_worker_request_t *my_worker_get_request_by_pid(
        my_worker_state_t *ws, int pid) 
{
    int i;
    my_worker_connection_t *conn;

    list_for_each_entry(conn, &ws->conns, entry) {
        for (i = 0; i < 65536; ++i) {
            if (conn->reqs[i] && conn->reqs[i]->fcgi_pid == pid)
                return conn->reqs[i];
        }
    }

    return NULL;
}

static int my_worker_request_flush_stderr_cb(void *r) {
    int rc;
    my_worker_request_t *req;
    my_callback_t *cb;

    req = (my_worker_request_t *)r;
    rc = my_worker_flush_stream(req, &req->fcgi_stderr_stream, 1);

    assert(!rc);

    return 0;
}


static int my_worker_request_terminate_cb(void *arg) {
    int rc;
    my_callback_t *cb, *tmp;
    my_worker_request_t *req;

    req = (my_worker_request_t *)arg;
    list_for_each_entry_safe(cb, tmp, &req->terminate_callbacks, entry) {
        rc = cb->func(cb->arg);

        if (rc) {
            cb = my_get_callback(&my_worker_request_terminate_cb, req);
            my_queue_push(&req->conn->waiting_for_freebuf_cbs_for_write, 
                    &cb->entry);
            return 1;
        } else {
            list_del(&cb->entry);
            free(cb);
        }
    }

    return 0;
}

static int my_worker_request_flush_stdout_cb(void *r) {
    int rc;
    my_worker_request_t *req;
    my_callback_t *cb;

    req = (my_worker_request_t *)r;
    rc = my_worker_flush_stream(req, &req->fcgi_stdout_stream, 1);

    assert(!rc);

    return 0;
}

static int my_worker_enqueue_end_request(my_worker_request_t *req) {
    my_buffer_t *buf;

    if (!(buf = my_get_buffer(&req->conn->write_pool)))
        return 1;

    buf->len = my_construct_fcgi_end_request(buf->buf, 
            req->id, req->fcgi_appstatus);

    my_log_debug("enqueued end-request start");

    my_queue_push(&req->conn->output_queue, &buf->entry);

    if (!req->conn->write_event_active)
        event_add(req->conn->write_event, NULL);

    my_log_debug("enqueued end-request done");

    return 0;
}

static int my_worker_enqueue_stdin_end_marker(my_worker_request_t *req) {
    my_buffer_t *buf;

    if (!(buf = my_get_buffer(&req->conn->read_pool)))
        return 1;

    buf->offs = -1;

    my_queue_push(&req->fcgi_stdin_stream.queue, &buf->entry);

    if (!req->fcgi_stdin_stream.active) {
        req->fcgi_stdin_stream.active = 1;
        event_add(req->fcgi_stdin_stream.ev, NULL);
    }

    return 0;
}

static void my_worker_handle_dead_children(my_worker_state_t *ws) {
    int rc, status;
    my_callback_t *cb;
    my_worker_request_t *req;

    my_log_info("my_worker_handle_dead_children()");

    while (1) {
        rc = waitpid(-1, &status, WNOHANG);

        my_log_info("waitpid rc=%d", rc);

        if (rc <= 0) {
            break;
        }

        if (req = my_worker_get_request_by_pid(ws, rc)) {
            req->fcgi_appstatus = WEXITSTATUS(status);

            my_log_info("fcgi-application pid=%d exited %d code", req->fcgi_pid,
                    req->fcgi_appstatus);

            req->fcgi_pid = -1; 

            cb = my_get_callback(&my_worker_request_flush_stdout_cb, req);
            my_queue_push(&req->terminate_callbacks, &cb->entry);

            cb = my_get_callback(&my_worker_request_flush_stderr_cb, req);
            my_queue_push(&req->terminate_callbacks, &cb->entry);

            cb = my_get_callback(&my_worker_enqueue_stdout_eof_cb, req);
            my_queue_push(&req->terminate_callbacks, &cb->entry);

            cb = my_get_callback(&my_worker_enqueue_stderr_eof_cb, req);
            my_queue_push(&req->terminate_callbacks, &cb->entry);

            cb = my_get_callback(&my_worker_enqueue_end_request_cb, req);
            my_queue_push(&req->terminate_callbacks, &cb->entry);

            my_worker_request_terminate_cb(req);
        }
    }
}

static void my_worker_wait_for_connections_shutdown(my_worker_state_t *state)
{
    my_worker_connection_t *c;
    struct timeval sleep_time = { 0, 100000 };
    struct timeval loop_time = { 1, 0 }; /* 2s */
    struct timeval tv_limit, tv_now;

    my_log_debug("waiting for connection shutdown");

    gettimeofday(&tv_limit, NULL);
    time_add(&tv_limit, &loop_time);

    while (!list_empty(&state->conns)) {
        event_base_loopexit(state->evbase, &sleep_time);
        event_base_loop(state->evbase, EVLOOP_ONCE);

        gettimeofday(&tv_now, NULL);
        if (time_after(&tv_now, &tv_limit)) {
            break;
        }
    }

    my_log_debug("waiting for connection shutdown done");
}

static int worker_main_cycle(void *arg) {
    my_master_state_t *master_state = (my_master_state_t *)arg;
    my_worker_state_t worker_state;
    my_worker_connection_t *conn, *tmp;
    struct timeval sleep_time = { 0, 100000 };

    my_worker_state_init(master_state, &worker_state);

    my_init_worker_signals();

    if (my_init_worker_events(&worker_state, master_state)) {
        return 1;
    }

    my_log_debug("worker %d started event-loop", getpid());

    while (!worker_should_stop) {
        event_base_loopexit(worker_state.evbase, &sleep_time);
        event_base_loop(worker_state.evbase, EVLOOP_ONCE);

        if (worker_should_handle_children) {
            my_worker_handle_dead_children(&worker_state);
            worker_should_handle_children = 0;
        }
    }

    my_log_debug("worker %d ended event-loop", getpid());

    list_for_each_entry(conn, &worker_state.conns, entry) {
        my_worker_close_connection(conn);
    }

    my_worker_wait_for_connections_shutdown(&worker_state);

    list_for_each_entry_safe(conn, tmp, &worker_state.conns, entry) {
        my_worker_abort_connection(conn);
    }

    return 0;
}


int main() {
    int rc, pid;

    set_logger_level(LOG_DEBUG);
    //set_logger_level(LOG_INFO);
    logger_init("/dev/stderr");

    struct event_base *evbase = event_base_new();
    if (!evbase) {
        my_log_crit("event_base_new() failed");
        return 1;
    }

    my_master_state_t state;    
    memset(&state, 0, sizeof(state));

    state.workers = 1;
    state.listen_port = 9170;
    state.worker_cycle_proc = &worker_main_cycle;
    state.fastcgi_path = "/home/akuts/stuff/experiments/hacks/fastcgi/1.cgi";

    if (my_master_state_init(&state) 
        || my_start_listening(&state))
        return 1;

    if (my_start_workers(&state))
        return 2;

    my_init_master_signals();

    if (my_wait_for_workers(&state, 1, 1))
        return 3;

    my_stop_workers(&state);

    if (my_wait_for_workers(&state, 0, 0))
        return 4;

    my_kill_workers(&state);

    my_master_state_deinit(&state);
    return 0;
}

