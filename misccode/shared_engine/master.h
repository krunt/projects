#ifndef MASTER_SE__
#define MASTER_SE__

#include "state.h"

void
sending_count(int fd, short event, void *arg);

void
begin_mastering(state_t state);

int
determine_master_pid(state_t state);

#endif
