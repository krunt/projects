#ifndef SHUTDOWN_SE__
#define SHUTDOWN_SE__

void
signal_callback(int fd, short event, void *arg);

struct state;

int
initialize_shutdown(struct state *state);

#endif
