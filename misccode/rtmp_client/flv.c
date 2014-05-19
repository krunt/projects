#include "flv.h"
#include <assert.h>

flv_context_t *my_flv_open(const char *path, const char *mode) {
    flv_context_t *res = malloc(sizeof(flv_context_t));
    if (!(res->fd = fopen(path, mode))) {
        free(res);
        res = NULL;
    }
    res->prev_tagsize = 0;
    return res;
}

int my_flv_write_header(flv_context_t *ctx) {
    u8 *p, buf[128];
    u32 data_offset = 9;

    p = buf;
    *p++ = 'F';
    *p++ = 'L';
    *p++ = 'V';
    *p++ = 1;
    *p++ = 5;
    pack4_be(p, &data_offset); p += 4;

    if (fwrite(buf, 1, p - buf, ctx->fd) != p - buf)
        return 1;

    return 0;
}

int my_flv_write(flv_context_t *ctx, my_packet_t *pkt) {
    u8 *p, b[4+1+3+3+1+3];

    p = b;
    pack4_be(p, &ctx->prev_tagsize); p += 4;

    switch (pkt->flags & PACKET_TYPE_MASK) {
    case PACKET_TYPE_AUDIO: *p++ = 8; break;
    case PACKET_TYPE_VIDEO: *p++ = 9; break;
    case PACKET_TYPE_METADATA: *p++ = 18; break;
    default: assert(0);
    };

    pack3_be(p, &pkt->size); p += 3;
    pack3_be(p, &pkt->pts); p += 3;
    *p++ = (pkt->pts >> 24) & 0xFF;
    *p++ = 0; *p++ = 0; *p++ = 0;

    if (fwrite(b, 1, sizeof(b), ctx->fd) != sizeof(b))
        return 1;

    if (fwrite(pkt->data, 1, pkt->size, ctx->fd) != pkt->size)
        return 1;

    ctx->prev_tagsize = pkt->size;

    return 0;
}

int my_flv_close(flv_context_t *ctx) {
    u8 b[4];
    if (ctx->prev_tagsize) {
        pack4_be(b, &ctx->prev_tagsize);
        if (fwrite(b, 1, 4, ctx->fd) != 4)
            return 1;
    }
    fclose(ctx->fd);
    free(ctx);
    return 0;
}
