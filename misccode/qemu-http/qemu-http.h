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
#ifndef QEMU_HTTP_H
#define QEMU_HTTP_H

#define DEFAULT_HTTP_PORT 8888
#define QEMU_MAX_HTTP_CONNECTIONS 16
#define HEADER_SIZE 4096
#define OUT_BUFFER_SIZE 4096*4

/* states */ 
#define NEW_REQUEST 0
#define IN_HEADERS  1

typedef struct {
    int fd;
    int open;
    int state;

    int pos;
    char in_buffer[HEADER_SIZE];

    int out_size;
    char out_buffer[OUT_BUFFER_SIZE];

    int page_complete;
    int page_pos;
    char page[HEADER_SIZE];
} QemuHttpConnection;

typedef struct {
    int listen_fd; 
    QemuHttpConnection clients[QEMU_MAX_HTTP_CONNECTIONS];

    int connections;
} QemuHttpState;

int qemu_http_init(void);

#endif
