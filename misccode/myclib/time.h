#ifndef MYCLIB_TIME_
#define MYCLIB_TIME_

#include <sys/time.h>

static inline int time_after(struct timeval *b2, struct timeval *b1) {
    if (b1->tv_sec == b2->tv_sec)
        return b1->tv_usec < b2->tv_usec;
    return b1->tv_sec < b2->tv_sec;
}

/* b1 += b2 */
static inline void time_add(struct timeval *b1, struct timeval *b2) {
    b1->tv_sec += b2->tv_sec;
    b1->tv_usec += b2->tv_usec;

    if (b1->tv_usec >= 1000000) {
        b1->tv_sec++;
        b1->tv_usec -= 1000000;
    }
}

#endif /* MYCLIB_TIME_ */
