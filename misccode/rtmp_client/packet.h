#ifndef PACKET_DEF_
#define PACKET_DEF_

#include <myclib/all.h>

#define PACKET_TYPE_VIDEO 1
#define PACKET_TYPE_AUDIO 2
#define PACKET_TYPE_METADATA 4
#define PACKET_TYPE_MASK 0xff

typedef struct my_packet_s {
    u64 pts;
    u64 dts;

    int stream_index;
    int flags;

    u8 *data;
    u32 size;
} my_packet_t;

#endif /* PACKET_DEF_ */
