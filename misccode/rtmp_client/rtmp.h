#ifndef RTMP_DEF_
#define RTMP_DEF_

#include <myclib/all.h>

#include "url.h"
#include "utils.h"
#include "flv.h"

#define RTMP_VERSION 3
#define RTMP_USER_CHUNK_STREAM_ID 5

#define RMTP_CHUNK_CHANNEL_COUNT 128

#define RTMP_MSG_SET_CHUNK_SIZE 1
#define RTMP_MSG_ABORT_MESSAGE 2
#define RTMP_MSG_ACKNOWLEDGE 3
#define RTMP_MSG_USER_CONTROL 4
#define RTMP_MSG_ACKNOWLEDGE_SIZE 5
#define RTMP_MSG_SET_PEER_BANDWIDTH 6
#define RTMP_MSG_CMD_AMF0 20
#define RTMP_MSG_CMD_AMF3 17
#define RTMP_MSG_DATA_AMF0 18
#define RTMP_MSG_DATA_AMF3 15
#define RTMP_MSG_AUDIO 8
#define RTMP_MSG_VIDEO 9
#define RTMP_MSG_AGG 22

#define RTMP_EVENT_STREAM_BEGIN 0
#define RTMP_EVENT_STREAM_EOF 1
#define RTMP_EVENT_STREAM_DRY 2
#define RTMP_EVENT_STREAM_PING_REQUEST 6
#define RTMP_EVENT_STREAM_PING_RESPONSE 7

typedef enum {
    RTMP_INIT,
    RTMP_HANDSHAKE_DONE,
    RTMP_HANDSHAKE_DONE2,
    RTMP_CONNECTED,
    RTMP_READY_TO_PLAY,
    RTMP_PLAYING,
} RTMPState;

typedef struct rtmp_message_s {
    struct list_head node;

    int timestamp;
    int timestamp_delta;
    int is_timestamp_extended; /* 0|1 */
    int length;
    int type_id;
    int streamid;
    int chunk_id;

    u8 *body;
    int size;
} rtmp_message_t;

typedef struct rtmp_chunk_channel_s {
    struct list_head msg_pending; /* pending messages */
    rtmp_message_t last_message;
} rtmp_chunk_channel_t;

typedef struct rtmp_state_s {
    RTMPState state;
    u32 chunk_size;

    char *remote_url;

    char *host;
    int port;
    char *app_name;
    char *stream_name;
    char *flv_path;

    my_url_proto_t *proto;
    rtmp_chunk_channel_t in_channels[RMTP_CHUNK_CHANNEL_COUNT];

    /* helper data */
    u8 *read_buf;
    int read_buf_size;

    int stream_ready;
    int tx_seq;

    flv_context_t *flv_context;

} rtmp_state_t;

int rtmp_handshake(rtmp_state_t *st);
int rtmp_send_connect(rtmp_state_t *st);
int rtmp_send_pong(rtmp_state_t *st, rtmp_message_t *m);
int rtmp_send_play(rtmp_state_t *st);
int rtmp_send_create_stream(rtmp_state_t *st);
int rtmp_state_init(rtmp_state_t *st);
void rtmp_state_free(rtmp_state_t *st);
int rtmp_send_message(rtmp_state_t *st, rtmp_message_t *msg, 
        int chunk_stream_id);
int  rtmp_receive_chunk(rtmp_state_t *st, rtmp_message_t **msgout);
void rtmp_message_free(rtmp_state_t *st, rtmp_message_t *msg);

int rtmp_set_chunksize(rtmp_state_t *st, rtmp_message_t *msg);
int rtmp_receive_cmd(rtmp_state_t *st, rtmp_message_t *msg);
int rtmp_process_control_message(rtmp_state_t *st, rtmp_message_t *msg);


#endif /* RTMP_DEF_ */
