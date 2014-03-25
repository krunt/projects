#ifndef LIBASYNC_DEF__
#define LIBASYNC_DEF__

#include <pthread.h>
#include "qemu-coroutine.h"

/* async-flags */
#define READ_FLAG 1
#define WRITE_FLAG 2

typedef void coroutine_fn (*handler_func_t)(void *arg);

typedef struct action_s {
    handler_func_t func;
    void *arg;

    Coroutine *coro;

    int fd;
    int flags;
    int ready;

    struct action_s *next;
}  action_t;


typedef struct action_queue_s {
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    action_t *head;
    action_t *tail;
} action_queue_t;


typedef struct async_state_s {
    int cur_actions;
    int max_actions;
    int wait_time; /* ms */
    action_t *head;

    action_queue_t queue;
} async_state_t;


void async_init(async_state_t *state);
void async_push(async_state_t *state, action_t *action);
void async_loop(async_state_t *state);
void async_wait_fd(int fd, int flags);
void async_destroy(async_state_t *state);

action_t *get_action(handler_func_t func);
void put_action(action_t *action);

#endif /* LIBASYNC_DEF__ */
