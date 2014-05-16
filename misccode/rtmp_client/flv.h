#ifndef FLV_DEF_
#define FLV_DEF_

#include <stdio.h>
#include "packet.h"

#include <myclib/all.h>

typedef struct flv_context_s {
    FILE *fd;
    u32 prev_tagsize;
} flv_context_t;

flv_context_t *my_flv_open(const char *path, const char *mode);
int my_flv_write_header(flv_context_t *ctx);
int my_flv_write(flv_context_t *ctx, my_packet_t *pkt);
int my_flv_close();

#endif /* FLV_DEF_ */
