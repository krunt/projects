#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/select.h>
#include <fcntl.h>

#include "libasync.h"

static pthread_key_t my_thread_state_key;

static void action_queue_init(action_queue_t *queue)
{
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);

    queue->head = NULL;
    queue->tail = NULL;
}

static void action_queue_destroy(action_queue_t *queue)
{
    pthread_cond_destroy(&queue->cond);
    pthread_mutex_destroy(&queue->mutex);
}

static void action_queue_push(action_queue_t *queue, action_t *action)
{
    pthread_mutex_lock(&queue->mutex);

    if (!queue->head) {
        queue->head = queue->tail = action;
    } else {
        queue->tail->next = action;
        queue->tail = action;
    }

    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

static action_t *action_queue_pop_blocking(action_queue_t *queue)
{
    action_t *result;

    pthread_mutex_lock(&queue->mutex);

    while (!queue->head) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }

    result = queue->head;
    queue->head = queue->head->next;

    if (!queue->head) {
        queue->head = queue->tail = NULL;
    }

    pthread_mutex_unlock(&queue->mutex);

    return result;
}

static action_t *action_queue_pop_nonblocking(action_queue_t *queue)
{
    action_t *result;

    pthread_mutex_lock(&queue->mutex);

    if (!queue->head) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }

    result = queue->head;
    queue->head = queue->head->next;

    if (!queue->head) {
        queue->head = queue->tail = NULL;
    }

    pthread_mutex_unlock(&queue->mutex);

    return result;
}

static action_t *action_queue_pop(action_queue_t *queue, int to_block)
{
    return to_block ? action_queue_pop_blocking(queue) 
        : action_queue_pop_nonblocking(queue);
}

void async_init(async_state_t *state) 
{
    state->cur_actions = 0;
    state->max_actions = 6;
    state->wait_time = 50;
    state->head = NULL;
    pthread_key_create(&my_thread_state_key, NULL);
    action_queue_init(&state->queue);
}

void async_push(async_state_t *state, action_t *action)
{
    action_queue_push(&state->queue, action);
}

static void wait_ready_fd(async_state_t *state) 
{
    int maxfd = -1, ready_cnt;
    fd_set rdset, wdset;
    struct timeval tmout;
    action_t *action;

    FD_ZERO(&rdset);
    FD_ZERO(&wdset);
    for (action = state->head; action; action = action->next) {
        if (action->flags & READ_FLAG)
            FD_SET(action->fd, &rdset);

        if (action->flags & WRITE_FLAG)
            FD_SET(action->fd, &wdset);

        maxfd = action->fd < maxfd ? maxfd : action->fd;
    }

    if (state->wait_time) {
        tmout.tv_sec = 0;
        tmout.tv_usec = 1000 * state->wait_time;
    }

    ready_cnt = select(maxfd + 1, &rdset, &wdset, NULL, 
            state->wait_time ? &tmout : NULL);

    for (action = state->head; action; action = action->next) {
        if (FD_ISSET(action->fd, &rdset) && (action->flags & READ_FLAG))
            action->ready = 1;

        if (FD_ISSET(action->fd, &wdset) && (action->flags & WRITE_FLAG))
            action->ready = 1;
    }
}

void async_loop(async_state_t *state)
{
    action_t *p, *new_actions, *action, **action_last;

    while (1) {

        /* get new ones */
        action_last = &new_actions;
        while (state->cur_actions < state->max_actions
            && (action = action_queue_pop(&state->queue, 0)))
        {
            *action_last = action;
            action_last = &action->next; 
            ++state->cur_actions;
        }

        *action_last = state->head;
        state->head = new_actions;
        
        /* process actions */
        for (action = state->head, 
             action_last = &state->head; 
             action; )
        {
            if (action->ready) {
                pthread_setspecific(my_thread_state_key, action);
                qemu_coroutine_enter(action->coro, action);
            }

            /* this is normal return, not through yield */
            if (action->ready) {
                p = action->next;
                put_action(action);
                action = p;

                --state->cur_actions;

            } else {
                *action_last = action;
                action_last = &action->next;
                action = action->next;
            }
        }

        *action_last = NULL;

        wait_ready_fd(state);
    }
}

void async_destroy(async_state_t *state)
{
    action_queue_destroy(&state->queue);
}

action_t *get_action(handler_func_t func) 
{
    action_t *action = (action_t *) malloc(sizeof(action_t));

    action->func = func;
    action->arg = NULL;
    action->coro = qemu_coroutine_create(func);
    action->fd = -1;
    action->flags = 0;
    action->ready = 1;
    action->next = NULL;

    return action;
}

void put_action(action_t *action)
{
    free(action);
}

void async_wait_fd(int fd, int flags)
{
    action_t *action = pthread_getspecific(my_thread_state_key);

    action->fd = fd;
    action->ready = 0;
    action->flags = flags;

    while (!action->ready) {
        qemu_coroutine_yield();
    }
}
