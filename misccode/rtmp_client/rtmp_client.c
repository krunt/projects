#include <stdio.h>
#include <assert.h>

#include <myclib/all.h>
#include <myos.h>

#include <signal.h>

#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

#include "rtmp.h"
#include "flv.h"

int  rtmp_client_wait_play(rtmp_state_t *st) {
    int rc;
    rtmp_message_t *msg;
    while (st->state != RTMP_PLAYING) {
        rc = rtmp_receive_chunk(st, &msg);
        if (rc == 1 || (rc == 2 && rtmp_process_message(st, msg)))
            return 1;
    }
    return 0;
}

int  rtmp_client_wait_publishing(rtmp_state_t *st) {
    int rc;
    rtmp_message_t *msg;
    while (st->state != RTMP_PUBLISHING) {
        rc = rtmp_receive_chunk(st, &msg);
        if (rc == 1 || (rc == 2 && rtmp_process_message(st, msg)))
            return 1;
    }
    return 0;
}

static rtmp_state_t *global_st;
void sigint_media_handler(int signum) {
    rtmp_state_free(global_st);
    exit(0);
}

int rtmp_receive_media(rtmp_state_t *st) {
    int rc, flags, to_stop;
    rtmp_message_t *msg;
    my_packet_t pkt;

#ifndef WIN32
    sigset_t curr_set, old_set;

    sigemptyset(&curr_set);
    sigaddset(&curr_set, SIGINT);
#endif

    signal(SIGINT, &sigint_media_handler);

    to_stop = 0;
    global_st = st;

    while (!to_stop) {
        rc = rtmp_receive_chunk(st, &msg);

        if (rc == 1)
            return 1;

        if (!rc)
            continue;
    
        flags = 0;
        switch (msg->type_id) {
        case RTMP_MSG_AUDIO:
            flags = PACKET_TYPE_AUDIO;
            break;
        case RTMP_MSG_VIDEO:
            flags = PACKET_TYPE_VIDEO;
            break;
        case RTMP_MSG_DATA_AMF0:
            flags = PACKET_TYPE_METADATA;
            break;
        default:
            break;
        };

        if (flags) {
            pkt.pts = pkt.dts = msg->timestamp;
            pkt.stream_index = 0;
            pkt.flags = flags;
            pkt.data = msg->body;
            pkt.size = msg->size;

#ifndef WIN32
            sigprocmask(SIG_SETMASK, &curr_set, &old_set);
#endif

            if (my_flv_write(st->flv_context, &pkt)) {
                my_log_error("error writing flv file");
                to_stop = 1;
            }

#ifndef WIN32
            sigprocmask(SIG_SETMASK, &old_set, &curr_set);
#endif
        }

        rtmp_message_free(st, msg);
    }

    return 0;
}

 /* ms */
static int get_ts_from_packet(rtmp_state_t *st, AVStream *stream,
        AVPacket *pkt, i64 *first_pts) 
{
    int result;

    i64 pts = AV_NOPTS_VALUE;
    if (pkt->pts != AV_NOPTS_VALUE) {
        pts = pkt->pts;
    } else if (pkt->dts != AV_NOPTS_VALUE) {
        pts = pkt->dts;
    }

    if (pts != AV_NOPTS_VALUE) {
        if (*first_pts == AV_NOPTS_VALUE) 
            *first_pts = pts;

        result = st->ts_send_base 
            + (pts - *first_pts) * av_q2d(stream->time_base) * 1000;
    } else {
        result = rtmp_gettime();
    }

    return result;
}

void *rtmp_send_thread(void *arg) {
    int type, pkt_ts;
    i64 first_pts = AV_NOPTS_VALUE;
    AVFormatContext *in_ctx;
    AVPacket packet;
    rtmp_state_t *st = (rtmp_state_t *)arg;

    my_log_debug("send_thread started");

    av_register_all();

    if (av_open_input_file(&in_ctx, st->flv_path, NULL, 0, NULL) < 0) {
        fprintf(stderr, "couldn't open %s\n", st->flv_path);
        return (void*)1;
    }

    while (av_read_frame(in_ctx, &packet) >= 0) {

        type = 0;
        switch (in_ctx->streams[packet.stream_index]->codec->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            type = RTMP_MSG_VIDEO;
            break;
        case AVMEDIA_TYPE_AUDIO:
            type = RTMP_MSG_AUDIO;
            break;
        default: assert(0);
        };

        if (!type) 
            continue;

        pkt_ts = get_ts_from_packet(st, in_ctx->streams[packet.stream_index], 
            &packet, &first_pts);

        my_log_debug("av_read_frame(%s) with ts=%d/size=%d", 
            type == RTMP_MSG_VIDEO ? "video" : "audio", pkt_ts,
            packet.size);

        if (pkt_ts > rtmp_gettime()) {
            (*myos()->sleep)(pkt_ts - rtmp_gettime());
        }

        if (rtmp_send_data(st, type, packet.data, packet.size, pkt_ts)) {
            my_log_debug("rmtp_send_data() failed");
            return (void*)1;
        }
    }

    rtmp_send_eof(st);

    av_close_input_file(in_ctx);
    return NULL;
}

void *rtmp_recv_thread(void *arg) {
    int rc;
    rtmp_state_t *st = (rtmp_state_t *)arg;
    rtmp_message_t *msg;

    while (1) {
        rc = rtmp_receive_chunk(st, &msg);

        if (rc == 1)
            return 1;

        if (!rc)
            continue;

        if (rtmp_process_message(st, msg))
            break;
    }

    return 0;
}

int rtmp_send_media(rtmp_state_t *st) {
    int rc;
    pthread_t rcv_thread;

    /* starting sending thread */
    if (pthread_create(&rcv_thread, NULL, &rtmp_recv_thread, (void *)st)
            || pthread_detach(rcv_thread))
        return 1;
    
    if (rtmp_send_thread(st))
        return 1;

    my_log_info("sent all data successfully");

    return 0;
}

int main(int argc, char **argv) {
    rtmp_state_t st;
    int to_write;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <url> <0|1>\n", argv[0]);
        return 1;
    }

    logger_init("/dev/stderr");
    //set_logger_level(LOG_DEBUG);
    set_logger_level(LOG_INFO);

    to_write = atoi(argv[2]) ? 1 : 0;
    st.remote_url = argv[1];
    st.flv_path = "dump.flv";

    if (rtmp_state_init(&st, to_write)) {
        my_log_error("rtmp_state_init() failed");
        return 1;
    }

    if (rtmp_handshake(&st)) {
        my_log_error("handshake is unsuccessful");
        return 1;
    }

    my_log_info("handshake is successful");

    if (rtmp_send_connect(&st)) {
        my_log_error("connect failed");
        return 1;
    }

    if (to_write) {
        if (rtmp_client_wait_publishing(&st))
            return 1;

        my_log_info("ready to send media");

        if (rtmp_send_media(&st)) {
            my_log_info("error sending media");
            return 1;
        }

    } else {
        if (rtmp_client_wait_play(&st))
            return 1;
    
        my_log_info("ready to play");
    
        if (rtmp_receive_media(&st)) {
            my_log_info("error getting media");
            return 1;
        }
    }

    rtmp_state_free(&st);
    return 0;
}

