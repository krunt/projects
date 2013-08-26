/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#define DEBUG_IRQ_COUNT

#include "qemu-http.h"
#include "qemu-common.h"
#include "qemu_socket.h"
#include "qemu-char.h"
#include "qerror.h"
#include "block.h"
#include "qint.h"
#include "qlist.h"
#include "qdict.h"

#include "hw/hw.h"
#include "hw/qdev.h"
#include "hw/usb.h"
#include "hw/pcmcia.h"
#include "hw/pc.h"
#include "hw/pci.h"
#include "hw/watchdog.h"
#include "hw/loader.h"
#include "gdbstub.h"
#include "net.h"
#include "net/slirp.h"
#include "qemu-char.h"
#include "ui/qemu-spice.h"
#include "sysemu.h"
#include "monitor.h"
#include "readline.h"
#include "console.h"
#include "blockdev.h"
#include "audio/audio.h"
#include "disas.h"
#include "balloon.h"
#include "qemu-timer.h"
#include "migration.h"
#include "kvm.h"
#include "acl.h"
#include "qint.h"
#include "qfloat.h"
#include "qlist.h"
#include "qbool.h"
#include "qstring.h"
#include "qjson.h"
#include "json-streamer.h"
#include "json-parser.h"
#include "osdep.h"
#include "cpu.h"
#include "tcg.h"
#include "disas.h"
#include "dis-asm.h"

static QemuHttpState *ghttp_state;

#define HTTP_OK "HTTP/1.1 200 OK\r\n"
#define NOT_FOUND "HTTP/1.1 404 Not Found\r\n"

#define CONTENT_LENGTH "Content-Length: %d\r\n"
#define CONTENT_TYPE_HTML "Content-Type: text/html; charset=UTF-8\r\n" 
#define CONTENT_TYPE_JSON "Content-Type: application/json;\r\n" 

#define COMMON_RESPONSE \
    "Connection: close\r\n" \
    "Server: Apache/1.3.3.7 (Unix) (Red-Hat/Linux)\r\n\r\n"


static int write_full(int fd, const char *buf, int size) {
    int w = 0;
    while (w < size) {
        int to_write = size - w > 1024 ? 1024 : size - w;
        int rc = write(fd, buf + w, to_write);
        if (rc == 0 || (rc == -1 && (errno != EAGAIN && errno != EINTR))) {
            return 1;
        }
        w += rc;
    }
    return 0;
}

static int response_request(QemuHttpConnection *conn) {
    return write_full(conn->fd, conn->out_buffer, conn->out_size);
}

static int response_with_file(QemuHttpConnection *conn, const char *filename) {
    FILE *fd = fopen(filename, "rt");

    if (!fd)
        return 1;

    if (fseek(fd, 0, SEEK_END) == -1)
        return 1;

    int filesize = ftell(fd);
    char *buf = qemu_malloc(filesize);
    rewind(fd);

    char *p = buf;
    int bytes_remain = filesize;
    int read_bytes;
    while (bytes_remain 
        && (read_bytes = fread(p, 1, bytes_remain > 1024 ? 1024 : bytes_remain, fd))) 
    {
        if (read_bytes == -1) {
            if (errno == EINTR) 
                continue;
            qemu_free(buf);
            return 1;
        }
        p += read_bytes;
        bytes_remain -= read_bytes;
    }
    fclose(fd);

    conn->out_size = snprintf(conn->out_buffer, 
        OUT_BUFFER_SIZE,
        HTTP_OK 
        CONTENT_TYPE_HTML
        CONTENT_LENGTH
        COMMON_RESPONSE, filesize);

    if (!response_request(conn) && !write_full(conn->fd, buf, filesize))
    {
        qemu_free(buf); 
        return 0;
    } else {
        qemu_free(buf); 
        return 1;
    }
}

static int respond_with_json(QemuHttpConnection *conn, QObject *obj) {
    QString *str = qobject_to_json(obj);

    conn->out_size = snprintf( conn->out_buffer, 
            OUT_BUFFER_SIZE,
            HTTP_OK 
            CONTENT_TYPE_JSON
            CONTENT_LENGTH
            COMMON_RESPONSE, str->length);

    int st = 0;
    if (!response_request(conn) && !write_full(conn->fd, str->string, str->length)) {
        st = 0;
    } else {
        st = 1;
    }

    qobject_decref(QOBJECT(str));

    return st;
}


static int process_memory_info(QemuHttpConnection *conn) {
    int l1, l2;
    int st;
    uint32_t pgd, pde, pte;
    int buf_size;
    char buf[128];
    CPUState *env = first_cpu;

    QList *res = qlist_new();

    if (!(env->cr[0] & CR0_PG_MASK))
        goto out;

    if (env->cr[4] & CR4_PAE_MASK)
        goto out;

    pgd = env->cr[3] & ~0xfff;
    for (l1 = 0; l1 < 1024; ++l1) {
        cpu_physical_memory_read(pgd + l1 * 4, &pde, 4);
        pde = le32_to_cpu(pde);
        if (pde & PG_PRESENT_MASK) {
            /* 4kb pages */
            if (!(pde & PG_PSE_MASK) || !(env->cr[4] & CR4_PSE_MASK)) {
                for (l2 = 0; l2 < 1024; ++l2) {
                    cpu_physical_memory_read((pde & ~0xfff) + l2 * 4, &pte, 4);
                    pte = le32_to_cpu(pte);
                    if (pte & PG_PRESENT_MASK) {
                        buf_size = snprintf(buf, sizeof(buf),
                            "%02X (%02X) (%c%c%c%c%c%c%c%c%c)",
                            (l1 << 22) + (l2 << 12), (pde & ~0xfff) + l2 * 4, 
                            pte & PG_NX_MASK ? 'X' : '-',
                            pte & PG_GLOBAL_MASK ? 'G' : '-',
                            pte & PG_PSE_MASK ? 'P' : '-',
                            pte & PG_DIRTY_MASK ? 'D' : '-',
                            pte & PG_ACCESSED_MASK ? 'A' : '-',
                            pte & PG_PCD_MASK ? 'C' : '-',
                            pte & PG_PWT_MASK ? 'T' : '-',
                            pte & PG_USER_MASK ? 'U' : '-',
                            pte & PG_RW_MASK ? 'W' : '-');
                        buf[buf_size] = 0;
                        qlist_append(res, qstring_from_str(buf));
                    }
                }
            }
        }
    }

out:
    st = respond_with_json(conn, QOBJECT(res));
    qobject_decref(QOBJECT(res));

    return st;
}

typedef struct {
    #define DISAS_BUFFER_SIZE 4096*4
    char buffer[DISAS_BUFFER_SIZE+1];
    int size;
} DisasBuffer;

static int
disas_fprintf(FILE *stream, const char *fmt, ...) {
    DisasBuffer *str = (DisasBuffer *) stream;

    va_list ap;
    va_start(ap, fmt);
    str->size += vsnprintf(str->buffer + str->size,
            DISAS_BUFFER_SIZE - str->size, fmt, ap);
    va_end(ap);
    return 0;
}

static 
void disassemble_to_buffer(TranslationBlock *tb, DisasBuffer *str) {
    unsigned long pc;
    struct disassemble_info disasm_info;
    int (*print_insn)(bfd_vma pc, disassemble_info *info);

    INIT_DISASSEMBLE_INFO(disasm_info, (FILE *)str, disas_fprintf);

    disasm_info.buffer = tb->tc_ptr;
    disasm_info.buffer_vma = (unsigned long)tb->tc_ptr;
    disasm_info.buffer_length = tb->size;

#ifdef HOST_WORDS_BIGENDIAN
    disasm_info.endian = BFD_ENDIAN_BIG;
#else
    disasm_info.endian = BFD_ENDIAN_LITTLE;
#endif
#if defined(__i386__)
    disasm_info.mach = bfd_mach_i386_i386;
    print_insn = print_insn_i386;
#elif defined(__x86_64__)
    disasm_info.mach = bfd_mach_x86_64;
    print_insn = print_insn_i386;
#elif defined(_ARCH_PPC)
    print_insn = print_insn_ppc;
#elif defined(__alpha__)
    print_insn = print_insn_alpha;
#elif defined(__sparc__)
    print_insn = print_insn_sparc;
#if defined(__sparc_v8plus__) || defined(__sparc_v8plusa__) || defined(__sparc_v9__)
    disasm_info.mach = bfd_mach_sparc_v9b;
#endif
#elif defined(__arm__)
    print_insn = print_insn_arm;
#elif defined(__MIPSEB__)
    print_insn = print_insn_big_mips;
#elif defined(__MIPSEL__)
    print_insn = print_insn_little_mips;
#elif defined(__m68k__)
    print_insn = print_insn_m68k;
#elif defined(__s390__)
    print_insn = print_insn_s390;
#elif defined(__hppa__)
    print_insn = print_insn_hppa;
#elif defined(__ia64__)
    print_insn = print_insn_ia64;
#else
    fprintf(out, "0x%lx: Asm output not supported on this arch\n",
	    (long) code);
    return;
#endif
    
    int icount = 0;
    int count = 0;
    for (pc = (unsigned long)tb->tc_ptr; 
            icount < tb->size && count < DISAS_BUFFER_SIZE; ) 
    {
        int before = str->size;
        print_insn(pc, &disasm_info);
        int after = str->size;

        if (after == DISAS_BUFFER_SIZE)
            break;

        icount += after - before;

        str->buffer[after++] = '\n';

        pc += after - before;
        count += after - before;
    }
    str->buffer[count] = 0;
}

static int process_disas_info(QemuHttpConnection *conn) {
    TranslationBlock *tb;
    DisasBuffer disas_buf = { {0}, 0 };

    QList *res = qlist_new();
    tb = &tbs[0];

    disassemble_to_buffer(tb, &disas_buf);
    qlist_append(res, qstring_from_str(disas_buf.buffer));

    int st = 0;
    st = respond_with_json(conn, QOBJECT(res));
    qobject_decref(QOBJECT(res));

    return st;
}

static int process_jit_info(QemuHttpConnection *conn) {
    int i;
    TranslationBlock *tb;

    QList *res = qlist_new();
    for (i = 0; i < nb_tbs; i++) {
        tb = &tbs[i];

        QList *item = qlist_new();
        qlist_append(item, qint_from_int(tb->pc));
        qlist_append(item, qint_from_int(tb->size));
        qlist_append(item, qint_from_int(tb->flags));
        qlist_append(item, qint_from_int(tb->page_addr[1] != -1 ? 1 : 0));
        qlist_append(item, qint_from_int(tb->tb_next_offset[0]));
        qlist_append(item, qint_from_int(tb->tb_next_offset[1]));

        qlist_append(res, item);
    }

    int st = 0;
    st = respond_with_json(conn, QOBJECT(res));
    qobject_decref(QOBJECT(res));

    return st;
}

static int process_irq_info(QemuHttpConnection *conn) {
    int i;
    int64_t count;

    QList *res = qlist_new();
    for (i = 0; i < 16; ++i) {
        count = irq_count[i];
        qlist_append(res, qint_from_int(count));
    }

    int st = 0;
    st = respond_with_json(conn, QOBJECT(res));
    qobject_decref(QOBJECT(res));

    return st;
}

static int process_cpu_info(QemuHttpConnection *conn) {
    CPUState *env;
    QList *cpu_list;
    int i;

    cpu_list = qlist_new();

    for (env = first_cpu; env != NULL; env = env->next_cpu) { 
        QDict *cpu;
        QObject *obj;

        obj = qobject_from_jsonf("{ 'CPU': %d }", env->cpu_index);

        cpu = qobject_to_qdict(obj);

        qdict_put(cpu, "eip", qint_from_int(env->eip + env->segs[R_CS].base));
        qdict_put(cpu, "eax", qint_from_int(EAX));
        qdict_put(cpu, "ebx", qint_from_int(EBX));
        qdict_put(cpu, "ecx", qint_from_int(ECX));
        qdict_put(cpu, "edx", qint_from_int(EDX));
        qdict_put(cpu, "esp", qint_from_int(ESP));
        qdict_put(cpu, "ebp", qint_from_int(EBP));
        qdict_put(cpu, "esi", qint_from_int(ESI));
        qdict_put(cpu, "edi", qint_from_int(EDI));

        qdict_put(cpu, "dt", qint_from_int(dev_time));
        qdict_put(cpu, "qt", qint_from_int(qemu_time));

#ifdef CONFIG_PROFILER
        qdict_put(cpu, "tb_count", qint_from_int(tcg_ctx.tb_count));
        qdict_put(cpu, "tb_count1", qint_from_int(tcg_ctx.tb_count1));
        qdict_put(cpu, "op_count", qint_from_int(tcg_ctx.op_count));
        qdict_put(cpu, "op_count_max", qint_from_int(tcg_ctx.op_count_max));
        qdict_put(cpu, "temp_count", qint_from_int(tcg_ctx.temp_count));
        qdict_put(cpu, "temp_count_max", qint_from_int(tcg_ctx.temp_count_max));
        qdict_put(cpu, "del_op_count", qint_from_int(tcg_ctx.del_op_count));
        qdict_put(cpu, "code_in_len", qint_from_int(tcg_ctx.code_in_len));
        qdict_put(cpu, "code_out_len", qint_from_int(tcg_ctx.code_out_len));
        qdict_put(cpu, "interm_time", qint_from_int(tcg_ctx.interm_time));
        qdict_put(cpu, "code_time", qint_from_int(tcg_ctx.code_time));
        qdict_put(cpu, "la_time", qint_from_int(tcg_ctx.la_time));
        qdict_put(cpu, "restore_count", qint_from_int(tcg_ctx.restore_count));
        qdict_put(cpu, "restore_time", qint_from_int(tcg_ctx.restore_time));

        for (i = INDEX_op_end; i < NB_OPS; i++) {
            qdict_put(cpu, tcg_op_defs[i].name, qint_from_int(tcg_table_op_count[i]));
        }
#endif

        qlist_append(cpu_list, cpu);
    }

    int st = 0;
    st = respond_with_json(conn, QOBJECT(cpu_list));
    qobject_decref(QOBJECT(cpu_list));

    return st;
}

static int process_request(QemuHttpConnection *conn) {
    if (!strncmp(conn->page, "/test\0", 6)) {
        conn->out_size = snprintf( conn->out_buffer, 
                OUT_BUFFER_SIZE,
                HTTP_OK 
                CONTENT_TYPE_JSON
                CONTENT_LENGTH
                COMMON_RESPONSE 
                "{\"dragon\": %d}", 13, 5);
    } else if (!strncmp(conn->page, "/cpu\0", 5)) {
        return process_cpu_info(conn);
    } else if (!strncmp(conn->page, "/irq\0", 5)) {
        return process_irq_info(conn);
    } else if (!strncmp(conn->page, "/memory\0", 8)) {
        return process_memory_info(conn);
    } else if (!strncmp(conn->page, "/jit\0", 5)) {
        return process_jit_info(conn);
    } else if (!strncmp(conn->page, "/disas\0", 7)) {
        return process_disas_info(conn);
    } else if (!strncmp(conn->page, "/index.html\0", 12)) {
        return response_with_file(conn, conn->page + 1);
    } else if (!strncmp(conn->page, "/jquery.js\0", 11)) {
        return response_with_file(conn, conn->page + 1);
    } else {
        conn->out_size = snprintf(conn->out_buffer, 
                OUT_BUFFER_SIZE,
                NOT_FOUND 
                CONTENT_TYPE_HTML 
                COMMON_RESPONSE);
    }
    if (response_request(conn)) {
        return 1;
    }
    return 0;
}

static int parse_page(QemuHttpConnection *conn) {
    char *p = conn->page;
    char *e = conn->page + HEADER_SIZE;

    /* GET, POST, ... */
    while (p < e && *p++ != ' ') {}

    if (p >= e)
        return 1;
    
    char *page_begin = p;
    char *page_end;
    while (p < e && *p++ != ' ') {}
    page_end = p - 2;

    if (p >= e)
        return 1;

    while (p < e && *p++ != ' ') {}

    p = page_end;
    while (p >= page_begin && *p != '/') {
        p--;
    }

    if (p < page_begin) 
    { p = page_begin; }

    memmove(conn->page, p, page_end - p + 1);
    conn->page[page_end - p + 1] = 0;

    return 0;
}

static void client_handler(void *opaque)
{
    QemuHttpConnection *conn = (QemuHttpConnection *)opaque;

    if (conn->state == NEW_REQUEST) {
        conn->pos = 0;
    }
    if (conn->pos == HEADER_SIZE) {
        conn->pos = 0;
    }

    int read_bytes 
        = read(conn->fd, conn->in_buffer + conn->pos, HEADER_SIZE - conn->pos);  

    /* connection is closed or invalid */
    if (read_bytes == 0 
            || (read_bytes == -1 && (errno != EINTR && errno != EAGAIN)))
    {
        goto end_connection;
    }

    if (conn->state == NEW_REQUEST) {
        conn->state = IN_HEADERS;
    }

    int b = conn->pos;
    int e = conn->pos + read_bytes;
    while (b < e) {
        /* we are at beginning ? */
        if (!conn->page_complete) {
            while (b < e) {
                if (conn->page_pos >= 0 && conn->page[conn->page_pos] == '\r'
                        && conn->in_buffer[b] == '\n')
                {
                    conn->page[++conn->page_pos] = conn->in_buffer[b++];
                    conn->page_complete = 1;
                    break;
                } else {
                    conn->page[++conn->page_pos] = conn->in_buffer[b++];
                }
            }
        } else {
            if (b >= 3) {
                if (conn->in_buffer[b-3] == '\r' && conn->in_buffer[b-2] == '\n'
                    && conn->in_buffer[b-1] == '\r' && conn->in_buffer[b] == '\n')
                {
                    conn->state = NEW_REQUEST;
                    goto process_request;
                }
            }
            b++;
        }
    }
    conn->pos = e;

    if (conn->state == IN_HEADERS) {
        return;
    }

process_request:
    if (parse_page(conn) || process_request(conn)) {
        goto end_connection;
    }

end_connection:
    close(conn->fd);             
    conn->open = 0;
    conn->state = NEW_REQUEST;
    ghttp_state->connections--;
    qemu_set_fd_handler(conn->fd, NULL, NULL, NULL);
}

static void accept_handler(void *opaque)
{
    QemuHttpState *state = (QemuHttpState *)opaque;

    if (state->connections == QEMU_MAX_HTTP_CONNECTIONS)
        return;

    int fd = qemu_accept(state->listen_fd, NULL, 0);

    if (fd < 0)
        return;

    state->connections++;
    
    int i;
    int mindex = 0;
    for (i = 0; i < QEMU_MAX_HTTP_CONNECTIONS; ++i) {
        if (!state->clients[i].open) {
            mindex = i;
            break;
        }
    }
    
    QemuHttpConnection *conn = &state->clients[mindex];
    conn->fd = fd;
    conn->open = 1;
    conn->state = NEW_REQUEST;
    conn->pos = 0;
    conn->page_complete = 0;
    conn->page_pos = -1;
    conn->out_size = 0;
    memset(conn->page, 0, sizeof(conn->page));

    fcntl_setfl(conn->fd, O_NONBLOCK);
    qemu_set_fd_handler(conn->fd, client_handler, NULL, (void *)conn);
}

int qemu_http_init(void) 
{
    QemuHttpState *state = (QemuHttpState *)qemu_mallocz(sizeof(QemuHttpState)); 
    ghttp_state = state;

    state->listen_fd = 0;
    memset(state->clients, 0, sizeof(state->clients));
    state->connections = 0;

    state->listen_fd = qemu_socket(AF_INET, SOCK_STREAM, 0);
    if (state->listen_fd < 0) {
        return 0;
    }

    const int on = 1;
    setsockopt(state->listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&on, sizeof(on));

    struct sockaddr_in laddr;
    memset(&laddr, 0, sizeof(laddr));
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(DEFAULT_HTTP_PORT);
    laddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(state->listen_fd, (struct sockaddr *)&laddr, sizeof(laddr)) == -1) {
        return 0;
    }

    if (listen(state->listen_fd, QEMU_MAX_HTTP_CONNECTIONS) == -1) {
        return 0;
    }

    fcntl_setfl(state->listen_fd, O_NONBLOCK);
    qemu_set_fd_handler(state->listen_fd, accept_handler, NULL, (void *)state);

    return 1;
}
