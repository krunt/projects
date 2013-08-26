
#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <pthread.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/thread.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include <mysql/mysql.h>

#include "compat/queue.h"
#include "compat/lua/lua.h"

#ifdef _EVENT_HAVE_NETINET_IN_H
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
# endif
#endif

// #define DEBUG 0
#undef DEBUG

const char *INTERPRETER_NAME = "/usr/bin/lua";

const char *SQL_USER = "lua";
const char *SQL_PASS = "pass";
const char *SQL_DATABASE = "lua";
const char *SQL_DEFAULT_TABLE = "lua_info";

const char *SQL_HOST = "localhost";
const int   SQL_PORT = 3306;

const char *dir = "sample";
const char *host = "127.0.0.1";
int port = 50000;

#define NUMBER_OF_THREADS 2
#define COMMON_SIZE 128

struct master_statistics {
    int number_of_requests;
    int total_time;
};

struct worker_statistics {
    int network_bytes_read;
    int network_bytes_written;
    int bytes_to_lua_read;
    int bytes_to_lua_written;
    int number_of_requests;
    int total_time;
};

struct work_item {
    SIMPLEQ_ENTRY(work_item) next;
    struct evhttp_request *request;
    int  terminate;
    char script_name[COMMON_SIZE];
    char fielddate[COMMON_SIZE];
    char project[COMMON_SIZE];
};

SIMPLEQ_HEAD(work_queue, work_item);

struct global_state {
    struct work_queue queue;
    pthread_t workers[NUMBER_OF_THREADS];
    struct master_statistics master_stat;
    struct worker_statistics statistics[NUMBER_OF_THREADS];
    pthread_mutex_t lock;
    pthread_cond_t  cond;
};

struct global_state_wrapper {
    int worker_index;
    struct global_state *state;
};

struct work_item *
create_item() {
    struct work_item *item = malloc(sizeof(struct work_item));
    memset(item, 0, sizeof(*item));
    return item;
}

void
enqueue_work_item(struct global_state *mstate, struct work_item *item) {
    pthread_mutex_lock(&mstate->lock);
    SIMPLEQ_INSERT_TAIL(&mstate->queue, item, next);
    pthread_cond_signal(&mstate->cond);
    pthread_mutex_unlock(&mstate->lock);
}

#define COLLECT_STAT

#if defined COLLECT_STAT

#define INIT_MEASURE struct timeval tv_from, tv_to
#define START_MEASURE gettimeofday(&tv_from, NULL)
#define STOP_MEASURE  gettimeofday(&tv_to, NULL)
#define REPORT_TIME(x) ((x) += timediff(&tv_from, &tv_to))

#else

#define INIT_MEASURE do { } while (0)
#define START_MEASURE do { } while (0)
#define STOP_MEASURE  do { } while (0)
#define REPORT_TIME(x) do { } while (0)

#endif

int
timediff(struct timeval *from, struct timeval *to) {
    int seconds_from = from->tv_sec;
    int seconds_to = to->tv_sec;
    int need_addition = 0;
    if (to->tv_usec < from->tv_usec) {
        need_addition = 1;
        seconds_to--;
    }
    return (seconds_to - seconds_from) * 1000000  
            + (to->tv_usec - from->tv_usec + (need_addition ? 1000000 : 0));
}

void
usage(int argc, char **argv) {
    printf("%s --host=<host> --port=<port> --dir=<dir>", argv[0]);
}

int
parse_options(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        char *p = argv[i];
        switch (*p++) {
            case '-': {
                switch (*p++) {
                    case '-': {
                        if (   p[0] == 'h' 
                            && p[1] == 'o' 
                            && p[2] == 's' 
                            && p[3] == 't' 
                            && p[4] == '=') 
                        {
                            host = p + 5;
                        }
                        if (   p[0] == 'p' 
                            && p[1] == 'o' 
                            && p[2] == 'r' 
                            && p[3] == 't' 
                            && p[4] == '=') 
                        {
                            port = atoi(p + 5);
                        }
                        if (   p[0] == 'd' 
                            && p[1] == 'i' 
                            && p[2] == 'r' 
                            && p[3] == '=') 
                        {
                            dir = p + 4;
                        }
                    }
                    case 'h':
                        if (p[1] == '=')
                            host = p + 2; 
                        break;
                    case 'p':
                        if (p[1] == '=')
                            port = atoi(p + 2);
                        break;
                    case 'd':
                        if (p[1] == '=')
                            dir = p + 2; 
                        break;
                    case '?':
                    default:
                        usage(argc, argv);
                        return 0;
                };
            };
        };
    }
    return 1;
}

void 
fatal_internal_error(int err) {
}

void
logit(const char *message) {
    /*
    FILE *fd = fopen("core.log", "w");
    fprintf(fd, "%s\n", message);
    */
    printf("%s\n", message);
}

int
fexists(const char *filename) {
    struct stat buf;
    if (stat(filename, &buf) == -1)
        return 0;
    return 1;
}

void
construct_query(char *buf, int bufsize, const char *fielddate, const char *project) {
    char format[1024];
    snprintf(format, sizeof(format), 
            "SELECT fielddate, project, line FROM lua_info %s %s %s %s",
            fielddate || project ? "WHERE" : "",
            fielddate ? " fielddate = '%s' " : "%s",
            fielddate && project ? " AND " : "",
            project ? " project = '%s' " : "%s");
    snprintf(buf, bufsize, format, fielddate ? fielddate : "", project ? project : "");
}

void *
worker_routine(void *arg) {
    struct global_state_wrapper *wrapper = (struct global_state_wrapper *)arg;
    int thread_index = wrapper->worker_index;
    struct global_state *mstate = wrapper->state;
    struct worker_statistics *stat = &mstate->statistics[thread_index];

    /* don't need it anymore */
    free(wrapper);

    MYSQL connection;
    mysql_init(&connection);

    MYSQL *desc;
    if (!(desc = mysql_real_connect(&connection,
                SQL_HOST, SQL_USER, SQL_PASS, SQL_DATABASE,
                SQL_PORT, NULL, 0)))
    {
        fprintf(stderr, "Unable to create mysql connection\n");
        return NULL;
    }

    int end = 0;
    while (!end) {
        struct work_item *item = NULL;

        pthread_mutex_lock(&mstate->lock);
    
        if (SIMPLEQ_EMPTY(&mstate->queue)) {
            pthread_cond_wait(&mstate->cond, &mstate->lock);
        }

        if (!SIMPLEQ_EMPTY(&mstate->queue)) {
            item = SIMPLEQ_FIRST(&mstate->queue);
            SIMPLEQ_REMOVE_HEAD(&mstate->queue, item, next);
        }
    
        pthread_mutex_unlock(&mstate->lock);

        if (!item)
            continue;

        if (item->terminate) {
            end = 1;
        } else {
            stat->number_of_requests++;

            char final_query[1024];
            construct_query(final_query, sizeof(final_query), 
                    item->fielddate[0] ? item->fielddate : NULL, 
                    item->project[0] ? item->project : NULL);
        
#ifdef DEBUG
            logit(final_query);
#endif
            struct evhttp_request *request = item->request;

            INIT_MEASURE;
            START_MEASURE;

            if (mysql_query(desc, final_query))
                goto internal_error;
        
            MYSQL_RES *res = mysql_store_result(desc);
            if (!res)
                goto internal_error;
        
            int pipe_sc[2];
            int pipe_cs[2];
            if (pipe(pipe_sc) == -1)
                goto internal_error;
            if (pipe(pipe_cs) == -1)
                goto internal_error;
        
            int pid = fork();
            if (pid == -1)
                goto internal_error;
        
            if (!pid) {
                close(pipe_sc[1]);
                close(pipe_cs[0]);
                dup2(pipe_sc[0], fileno(stdin));     
                dup2(pipe_cs[1], fileno(stdout));
                const char *argv[3] = { INTERPRETER_NAME, item->script_name, NULL };
                execv(INTERPRETER_NAME, (char *const *)argv);
            } 
        
            close(pipe_sc[0]);
            close(pipe_cs[1]);
        
            FILE *fd = fdopen(pipe_sc[1], "w");
            char **p;
            while ((p = mysql_fetch_row(res))) {
                int bytes_read = strlen(p[0]) + strlen(p[1]) + strlen(p[2]);
                stat->network_bytes_read += bytes_read;
                fprintf(fd, "%s\t%s\t%s\n", p[0], p[1], p[2]);
                stat->bytes_to_lua_written += bytes_read + 3;
                fflush(fd);
            }
            fclose(fd);
        
            struct evbuffer *send_buf = evbuffer_new();
            int len;
            char cache_buf[1024];
            while ((len = read(pipe_cs[0], cache_buf, sizeof(cache_buf))) > 0) {
                evbuffer_add(send_buf, cache_buf, len);  
                stat->bytes_to_lua_read += len;
                stat->network_bytes_written += len;
            }
        
            int status;
            if (waitpid(pid, &status, 0) == -1)
                goto internal_error;

            close(pipe_sc[1]);
            close(pipe_cs[0]);
        
            evhttp_send_reply(request, HTTP_OK, "OK", send_buf);
            evbuffer_free(send_buf);
            mysql_free_result(res);

            STOP_MEASURE;
            REPORT_TIME(stat->total_time);

            goto ok;

        internal_error:
            do {
#ifdef DEBUG
            char message[1024];
            snprintf(message, sizeof(message), "Internal error: (%s)\n", strerror(errno));
            evhttp_send_error(request, HTTP_INTERNAL, message);
#else
            evhttp_send_error(request, HTTP_INTERNAL, "Internal error");
#endif
            } while (0);
        }

        ok:
        if (item) {
            free(item);
        }
    }

    mysql_close(desc);
    return NULL;
}

/* /get_script/1.lua */
void
process_get_script(struct evhttp_request *req, const char *filename) { 
    if (!strlen(filename))
        goto not_found;

    int fd;
    if ((fd = open(filename, O_RDONLY)) == -1)
        goto not_found;

    struct stat statbuf;
    if (fstat(fd, &statbuf) == -1)
        goto not_found;

    /* improve performance here */
    struct evbuffer *buffer = evbuffer_new();
    if (!buffer)
        goto internal_error;

    if (evbuffer_add_file(buffer, fd, 0, statbuf.st_size) == -1) {
        evbuffer_free(buffer);
        goto internal_error;
    }

    evhttp_send_reply(req, HTTP_OK, "OK", buffer);
    evbuffer_free(buffer);

    /* success */
    return;

not_found:
    evhttp_send_error(req, HTTP_NOTFOUND, "There is no such filename");
    return;

internal_error:
    evhttp_send_error(req, HTTP_INTERNAL, "Internal error");
}

void
report_stat(struct evhttp_request *request, struct global_state *mstate) {
    struct evbuffer *buffer = evbuffer_new();
    if (!buffer)
        goto internal_error;
    struct master_statistics *mstat = &mstate->master_stat;

    char statictics_report[1024];
    snprintf(statictics_report, sizeof(statictics_report),
            "Master statistics:<br>"
            "-- number-of-requests: %d<br>"
            "-- total_time:         %6.6f<br><br>", 
            mstat->number_of_requests, 
            (double)mstat->total_time / 1000000.0);
    evbuffer_add(buffer, statictics_report, strlen(statictics_report));

    for (int i = 0; i < NUMBER_OF_THREADS; ++i) {
        struct worker_statistics *wstat = &mstate->statistics[i];
        snprintf(statictics_report, sizeof(statictics_report),
            "Worker<%d> statistics:<br>"
            "-- number-of-requests:    %d<br>"
            "-- total_time:            %6.6f<br>"
            "-- network_bytes_read:    %d<br>"
            "-- network_bytes_written: %d<br>"
            "-- bytes_to_lua_read:     %d<br>"
            "-- bytes_to_lua_written:  %d<br><br>",
            i, wstat->number_of_requests, 
            (double)wstat->total_time / 1000000.0,
            wstat->network_bytes_read,
            wstat->network_bytes_written,
            wstat->bytes_to_lua_read,
            wstat->bytes_to_lua_written);
        evbuffer_add(buffer, statictics_report, strlen(statictics_report));
    }

    evhttp_send_reply(request, HTTP_OK, "OK", buffer);
    evbuffer_free(buffer);

    return; 

internal_error:
    evhttp_send_error(request, HTTP_INTERNAL, "Internal error");
}

void
process_calculate(struct evhttp_request *request, 
        const char *query, struct global_state *mstate) 
{
    const char *script_name = NULL;
    const char *fielddate = NULL;
    const char *project = NULL;

    struct work_item *item = create_item();
    if (!item)
        goto internal_error;

    item->request = request;

    struct evkeyvalq headers;
    if (evhttp_parse_query_str(query, &headers) == -1) {
        free(item);
        goto internal_error;
    }

    struct evkeyval *header;
    TAILQ_FOREACH(header, &headers, next) {
        if (!strcmp(header->key, "script")) {
            script_name = header->value;
            strncpy(item->script_name, script_name, sizeof(item->script_name));
        } else if (!strcmp(header->key, "fielddate")) {
            fielddate = header->value;
            strncpy(item->fielddate, fielddate, sizeof(item->fielddate));
        } else if (!strcmp(header->key, "project")) {
            project = header->value;
            strncpy(item->project, project, sizeof(item->project));
        }
    }

#ifdef DEBUG
    {
        char buf[1024];
        sprintf(buf, "(%p %s) (%p %s) (%p %s)", 
                script_name, script_name ? script_name : "<NONE>", 
                fielddate, fielddate ? fielddate : "<NONE>", 
                project, project ? project : "<NONE>");
        logit(buf);
    }
#endif

    if (!script_name || !fexists(script_name)) {
        free(item);
        goto internal_error;
    }
    
    enqueue_work_item(mstate, item);

    return;

internal_error:
    do {
#ifdef DEBUG
    char message[1024];
    snprintf(message, sizeof(message), "Internal error: (%s)\n", strerror(errno));
    evhttp_send_error(request, HTTP_INTERNAL, message);
#else
    evhttp_send_error(request, HTTP_INTERNAL, "Internal error");
#endif
    } while (0);
}

void
process_request(struct evhttp_request *request, void *arg) {
    struct global_state *mstate = (struct global_state *)arg;
    const struct evhttp_uri *elems = evhttp_request_get_evhttp_uri(request);
    const char *uri = evhttp_uri_get_path(elems);

    mstate->master_stat.number_of_requests++;

    INIT_MEASURE;
    START_MEASURE;

    if (!strncmp(uri, "/get_script/", 12)) {
        process_get_script(request, uri + 12); 
    } else if (!strncmp(uri, "/set_script/", 12)) {
        /* process_set_script(request, uri + 12); */
    } else if (!strncmp(uri, "/calculate", 10)) {
        process_calculate(request, evhttp_uri_get_query(elems), mstate);
    } else if (!strncmp(uri, "/report_stat", 12)) {
        report_stat(request, mstate);
    }

    STOP_MEASURE;
    REPORT_TIME(mstate->master_stat.total_time);
}

int
main(int argc, char **argv) {
    struct event_config *config;
    struct event_base *base;
	struct evhttp *http;
	struct evhttp_bound_socket *handle;
    struct global_state mstate;

    if (!parse_options(argc, argv)) {
        return -1;
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        return -2;
    }

    if (NUMBER_OF_THREADS < 1) {
        return -3;
    }

    evthread_use_pthreads();
    memset(&mstate.master_stat, 0, sizeof(mstate.master_stat));
    memset(&mstate.statistics, 0, sizeof(mstate.statistics));
    SIMPLEQ_INIT(&mstate.queue);

    pthread_mutex_init(&mstate.lock, NULL);
    pthread_cond_init(&mstate.cond, NULL);
    for (int i = 0; i < NUMBER_OF_THREADS; ++i) {
        struct global_state_wrapper *wrapper 
            = malloc(sizeof(struct global_state_wrapper));
        wrapper->worker_index = i;
        wrapper->state = &mstate;
        pthread_create(&mstate.workers[i], NULL, worker_routine, (void*)wrapper);
    }

    config = event_config_new();
    if (!config || event_config_avoid_method(config, "select") == -1) {
		fprintf(stderr, "Unable to init config\n");
		return 1;
    }

	base = event_base_new_with_config(config);
	if (!base) {
		fprintf(stderr, "Couldn't create an event_base: exiting\n");
		return 1;
	}

	http = evhttp_new(base);
	if (!http) {
		fprintf(stderr, "couldn't create evhttp. Exiting.\n");
		return 1;
	}

    event_set_fatal_callback(fatal_internal_error);
	evhttp_set_gencb(http, process_request, (void*)&mstate);

	handle = evhttp_bind_socket_with_handle(http, host, port);
	if (!handle) {
		fprintf(stderr, "couldn't bind to port %d. Exiting.\n",
		    (int)port);
		return 1;
	}

    printf("Listening on %s:%d <%s>...\n", host, port, dir);

    event_base_dispatch(base);

    for (int i = 0; i < NUMBER_OF_THREADS; ++i) {
        struct work_item *item = create_item();
        item->terminate = 1;
        enqueue_work_item(&mstate, item);
    }

    for (int i = 0; i < NUMBER_OF_THREADS; ++i) {
        pthread_join(mstate.workers[i], NULL);
    }

    pthread_cond_destroy(&mstate.cond);
    pthread_mutex_destroy(&mstate.lock);

    event_config_free(config);
    evhttp_free(http);
    event_base_free(base);
            
    return 0;
}
