/*
 * Copyright (C) 2007 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (C) 2009 Konstantin Shishkov
 * based on public domain SHA-1 code by Steve Reid <steve@edmweb.com>
 * and on BSD-licensed SHA-2 code by Aaron D. Gifford
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <include/common.h>

namespace btorrent {

static void sha1_transform(u32 state[5], const u8 buffer[64]);

sha1_hash_t::sha1_hash_t(const std::string &digest) {
    const std::string decoded = hex_decode(digest);
    init();
    memcpy(m_ctx.digest, decoded.data(), 20);
}

void sha1_hash_t::init() {
    memset(&m_ctx, 0, sizeof(m_ctx));
    m_ctx.state[0] = 0x67452301;
    m_ctx.state[1] = 0xEFCDAB89;
    m_ctx.state[2] = 0x98BADCFE;
    m_ctx.state[3] = 0x10325476;
    m_ctx.state[4] = 0xC3D2E1F0;
}

void sha1_hash_t::update(const char *data, size_type len) {
    unsigned int i, j;

    j = m_ctx.count & 63;
    m_ctx.count += len;
    if ((j + len) > 63) {
        memcpy(&m_ctx.buffer[j], data, (i = 64 - j));
        sha1_transform(m_ctx.state, m_ctx.buffer);
        for (; i + 63 < len; i += 64)
            sha1_transform(m_ctx.state, (const u8 *)&data[i]);
        j = 0;
    } else
        i = 0;
    memcpy(&m_ctx.buffer[j], &data[i], len - i);
}

void sha1_hash_t::update(const std::string &s) {
    update(s.data(), s.size());
}

void sha1_hash_t::finalize() {
    u64 finalcount = htobe64(m_ctx.count);
    update("\200", 1);
    while ((m_ctx.count & 63) != 56)
        update("", 1);
    update((const char *)&finalcount, 8); /* Should cause a transform() */
    for (int i = 0; i < 5; i++)
        int4store(m_ctx.digest + i*4, m_ctx.state[i]);
}

std::string sha1_hash_t::get_digest() const {
    return std::string((const char*)m_ctx.digest, 20);
}

bool sha1_hash_t::operator==(const sha1_hash_t &other) const {
    return get_digest() == other.get_digest();
}

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define blk0(i) (block[i] = htobe32(((const u32*)buffer)[i]))
#define blk(i)  (block[i] = rol(block[i-3] ^ block[i-8] ^ block[i-14] ^ block[i-16], 1))

#define R0(v,w,x,y,z,i) z += ((w&(x^y))^y)     + blk0(i) + 0x5A827999 + rol(v, 5); w = rol(w, 30);
#define R1(v,w,x,y,z,i) z += ((w&(x^y))^y)     + blk (i) + 0x5A827999 + rol(v, 5); w = rol(w, 30);
#define R2(v,w,x,y,z,i) z += ( w^x     ^y)     + blk (i) + 0x6ED9EBA1 + rol(v, 5); w = rol(w, 30);
#define R3(v,w,x,y,z,i) z += (((w|x)&y)|(w&x)) + blk (i) + 0x8F1BBCDC + rol(v, 5); w = rol(w, 30);
#define R4(v,w,x,y,z,i) z += ( w^x     ^y)     + blk (i) + 0xCA62C1D6 + rol(v, 5); w = rol(w, 30);

void sha1_transform(u32 state[5], const u8 buffer[64])
{
    u32 block[80];
    unsigned int i, a, b, c, d, e;

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    for (i = 0; i < 15; i += 5) {
        R0(a, b, c, d, e, 0 + i);
        R0(e, a, b, c, d, 1 + i);
        R0(d, e, a, b, c, 2 + i);
        R0(c, d, e, a, b, 3 + i);
        R0(b, c, d, e, a, 4 + i);
    }
    R0(a, b, c, d, e, 15);
    R1(e, a, b, c, d, 16);
    R1(d, e, a, b, c, 17);
    R1(c, d, e, a, b, 18);
    R1(b, c, d, e, a, 19);
    for (i = 20; i < 40; i += 5) {
        R2(a, b, c, d, e, 0 + i);
        R2(e, a, b, c, d, 1 + i);
        R2(d, e, a, b, c, 2 + i);
        R2(c, d, e, a, b, 3 + i);
        R2(b, c, d, e, a, 4 + i);
    }
    for (; i < 60; i += 5) {
        R3(a, b, c, d, e, 0 + i);
        R3(e, a, b, c, d, 1 + i);
        R3(d, e, a, b, c, 2 + i);
        R3(c, d, e, a, b, 3 + i);
        R3(b, c, d, e, a, 4 + i);
    }
    for (; i < 80; i += 5) {
        R4(a, b, c, d, e, 0 + i);
        R4(e, a, b, c, d, 1 + i);
        R4(d, e, a, b, c, 2 + i);
        R4(c, d, e, a, b, 3 + i);
        R4(b, c, d, e, a, 4 + i);
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

}

