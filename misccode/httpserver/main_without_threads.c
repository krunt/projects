
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

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
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

#define DEBUG

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

struct state {
    MYSQL connection;
    MYSQL *desc;
};

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

void
process_calculate(struct evhttp_request *request, 
        const char *query, struct state *mstate) 
{
    struct evkeyvalq headers;

    if (evhttp_parse_query_str(query, &headers) == -1)
        goto internal_error;

    const char *script_name = NULL;
    const char *fielddate = NULL;
    const char *project = NULL;

    struct evkeyval *header;
    TAILQ_FOREACH(header, &headers, next) {
        if (!strcmp(header->key, "script")) {
            script_name = header->value;
        } else if (!strcmp(header->key, "fielddate")) {
            fielddate = header->value;
        } else if (!strcmp(header->key, "project")) {
            project = header->value;
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

    if (!script_name || !fexists(script_name))
        goto internal_error;

    char final_query[1024];
    construct_query(final_query, sizeof(final_query), fielddate, project);

#ifdef DEBUG
    logit(final_query);
#endif

    if (mysql_query(mstate->desc, final_query))
        goto internal_error;

    MYSQL_RES *res = mysql_store_result(mstate->desc);
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
        const char *argv[3] = { INTERPRETER_NAME, script_name, NULL };
        execv(INTERPRETER_NAME, (char *const *)argv);
    } 

    close(pipe_sc[0]);
    close(pipe_cs[1]);

    FILE *fd = fdopen(pipe_sc[1], "w");
    char **p;
    while ((p = mysql_fetch_row(res))) {
        fprintf(fd, "%s\t%s\t%s\n", p[0], p[1], p[2]);
        fflush(fd);
    }
    fclose(fd);

    struct evbuffer *send_buf = evbuffer_new();
    int len;
    char cache_buf[1024];
    while ((len = read(pipe_cs[0], cache_buf, sizeof(cache_buf))) > 0) {
        evbuffer_add(send_buf, cache_buf, len);  
    }

    int status;
    if (waitpid(pid, &status, 0) == -1)
        goto internal_error;

    close(pipe_sc[1]);
    close(pipe_cs[0]);

    evhttp_send_reply(request, HTTP_OK, "OK", send_buf);
    evbuffer_free(send_buf);
    mysql_free_result(res);

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
    struct state *mstate = (struct state *)arg;
    const struct evhttp_uri *elems = evhttp_request_get_evhttp_uri(request);
    const char *uri = evhttp_uri_get_path(elems);
    if (!strncmp(uri, "/get_script/", 12)) {
        process_get_script(request, uri + 12); 
    } else if (!strncmp(uri, "/set_script/", 12)) {
        /* process_set_script(request, uri + 12); */
    } else if (!strncmp(uri, "/calculate", 10)) {
        process_calculate(request, evhttp_uri_get_query(elems), mstate);
    }
}

int
main(int argc, char **argv) {
    struct event_config *config;
    struct event_base *base;
	struct evhttp *http;
	struct evhttp_bound_socket *handle;
    struct state mstate;

    if (!parse_options(argc, argv)) {
        return -1;
    }

    mysql_init(&mstate.connection);
    if (!(mstate.desc = mysql_real_connect(&mstate.connection, 
                SQL_HOST, SQL_USER, SQL_PASS, SQL_DATABASE,
                SQL_PORT, NULL, 0)))
    {
        fprintf(stderr, "Unable to create mysql connection\n");
        return 1;
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

    event_config_free(config);
    evhttp_free(http);
    event_base_free(base);
    mysql_close(mstate.desc);
            
    return 0;
}
