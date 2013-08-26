#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct zstream_s {
    char *next_in;
    int avail_in;  
    char *next_out;
    int avail_out;
} zstream_t;


#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define WINDOWHALF_SIZE 32768
#define WINDOW_SIZE 2*WINDOWHALF_SIZE 

#define REP_3_6 16
#define REPZ_3_10 17
#define REPZ_11_138 18
#define END_BLOCK 256

#define DYN_TREES 2

#define MIN_MATCH 3
#define MAX_MATCH 258
#define MIN_LOOKAHEAD (MIN_MATCH+MAX_MATCH)

#define MAX_DIST(s) ((s)->wsize-MIN_LOOKAHEAD)

#define LENGTH_CODES 29
#define LITERALS  256

#define L_CODES (LITERALS+1+LENGTH_CODES)
#define D_CODES   30
#define BL_CODES  19
#define HEAP_SIZE (2*L_CODES+1)

#define MAX_BITS 15
#define MAX_BL_BITS 7
#define HASH_BITS 16
#define NIL 0

#define BUFFER_SIZE 128*1024
#define PENDING_BUF_SIZE 1024

#define CODE_ABSENCE -1

#define LENGTH_CODES 29
#define DIST_CODE_LEN 512

#define INBUF_SIZE PENDING_BUF_SIZE 

#define INSTREAM_NEED_REFILL -1
#define OUTSTREAM_NEED_REFILL -2

int extra_lbits[LENGTH_CODES]
   = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};

int extra_dbits[D_CODES]
   = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

int extra_blbits[BL_CODES]
    = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,3,7};

int bl_order[BL_CODES]
   = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

int _length_code[MAX_MATCH];
int _dist_code[DIST_CODE_LEN];
int base_length[LENGTH_CODES];
int base_dist[D_CODES];

typedef struct bit_stream_s {
    char inbuf[INBUF_SIZE];
    int inbuf_pointer;
    int inbuf_size;
    int cached_val;
    int bits_avail;
    zstream_t *stream;
    int error;
} bit_stream_t;

typedef struct tree_element_s {
    union {
        int freq;
        int code;
    } fc;
    union {
        int dad;
        int len;
    } dl;
} tree_element_t;

#define Freq fc.freq
#define Code fc.code
#define Dad  dl.dad
#define Len  dl.len

typedef struct decompression_state_s {
    char window[WINDOW_SIZE];

    int window_size;
    int wsize;

    int strstart;

    bit_stream_t bs;

    tree_element_t dyn_ltree[HEAP_SIZE];
    tree_element_t dyn_dtree[2*D_CODES+1];
    tree_element_t bl_tree[2*BL_CODES+1];

    int rev_ltree[HEAP_SIZE];
    int rev_dtree[2*D_CODES+1];
    int rev_bltree[2*BL_CODES+1];

    int bl_count[MAX_BITS+1];
} decompression_state_t;

typedef struct static_tree_desc_s {
    int *extra_bits;
    int extra_base;
    int elems;
    int max_length;
} static_tree_desc_t;

typedef struct tree_desc_s {
    tree_element_t *tree;
    int *rev_tree;
    int len;
    int max_code;
    static_tree_desc_t *sdesc;
} tree_desc_t;

static_tree_desc_t static_ldesc = 
{ extra_lbits, LITERALS+1, L_CODES, MAX_BITS };

static_tree_desc_t static_ddesc = 
{ extra_dbits, 0, D_CODES, MAX_BITS };

static_tree_desc_t static_bldesc = 
{ extra_blbits, 0, BL_CODES, MAX_BL_BITS };

int bi_reverse(int code, int len)
{
    int res = 0;
    do {
        res |= code & 1;
        code >>= 1, res <<= 1;
    } while (--len > 0);
    return res >> 1;
}

void init_bit_stream(bit_stream_t *bs, zstream_t *stream) {
    memset(bs, 0, sizeof(bit_stream_t)); 
    bs->stream = stream;
    bs->inbuf_pointer = 0;
    bs->inbuf_size = 0;
    bs->cached_val = 0;
    bs->bits_avail = 0;
    bs->error = 0;
}

int need_stream_bytes(bit_stream_t *bs, int count) {
    int bytes_avail = bs->inbuf_size - bs->inbuf_pointer;
    int to_read;
    if (bytes_avail >= count) {
        return 0;
    }
    if (bs->error) { return -1; }
    count -= bytes_avail;
    bs->error = 0; 
    if (bs->stream->avail_in >= count) {
        memmove(bs->inbuf, bs->inbuf + bs->inbuf_pointer, bytes_avail);
        bs->inbuf_pointer = 0;
        bs->inbuf_size = bytes_avail;
        to_read = MIN(bs->stream->avail_in, INBUF_SIZE - bs->inbuf_size);

        assert(to_read >= count);

        memcpy(bs->inbuf + bs->inbuf_size, bs->stream->next_in, to_read);
        bs->stream->next_in += to_read;
        bs->stream->avail_in -= to_read;
        bs->inbuf_size += to_read;
    } else {
        bs->error = INSTREAM_NEED_REFILL;
    }
    return bs->error ? -1 : 0;
}

int fill_cached_byte(bit_stream_t *bs) {
    if (bs->inbuf_pointer == bs->inbuf_size)
        if (need_stream_bytes(bs, 1))
            return -1;
    assert(bs->inbuf_pointer < bs->inbuf_size);
    bs->cached_val = bi_reverse(bs->inbuf[bs->inbuf_pointer++], 8);
    bs->bits_avail = 8;
    return 0;
}

int get_bits(bit_stream_t *bs, int bits) {
    assert(bits <= sizeof(int) * 8);
    int res = 0;
    int bytes_avail, bits_needed;

    bytes_avail = bs->inbuf_size - bs->inbuf_pointer;
    bits_needed = bits - bs->bits_avail - bytes_avail * 8;
    if (bits_needed > 0) {
        if (need_stream_bytes(bs, (bits_needed + 7)/8))
            return -1;
    }

    if (bits >= bs->bits_avail) {
        res = bs->cached_val & ((1<<bs->bits_avail)-1);
        bits -= bs->bits_avail;
        if (fill_cached_byte(bs))
            return -1;
    }

    while (bits >= 8) {
        res = (res << 8) | bs->cached_val;
        bits -= 8;
        if (fill_cached_byte(bs))
            return -1;
    }

    assert(bits < bs->bits_avail);
    if (bits) {
        res = (res << bits) | (bs->cached_val & ((1<<bits)-1));
        bs->bits_avail -= bits;
    }
    return res;
}

int get_code(bit_stream_t *bs, tree_desc_t *tree) {
    int *rev_tree = tree->rev_tree;
    int res = 0;
    int curlen = 0;
    while (1) {
        res |= get_bits(bs, 1);
        curlen++;
        if (rev_tree[res] != CODE_ABSENCE) {
            /* assert(tree->tree[rev_tree[res]].Len == curlen); */
            return res;
        }
        res <<= 1;
    }
    return res;
}

void myinflate_init_static_tables() 
{
    int length, code, dist, n;
    length = 0; 
    for (code = 0; code < LENGTH_CODES - 1; ++code) {
        base_length[code] = length;
        for (n = 0; n < (1<<extra_lbits[code]); ++n) {
            _length_code[length++] = code;
        }
    }

    dist = 0;
    for (code = 0; code < 16; ++code) {
        base_dist[code] = dist;
        for (n = 0; n < (1<<extra_dbits[code]); ++n) {
            _dist_code[dist++] = code;
        }
    }

    dist >>= 7;
    for ( ; code < D_CODES; code++) {
        base_dist[code] = dist << 7;
        for (n = 0; n < (1<<(extra_dbits[code]-7)); n++) {
            _dist_code[256 + dist++] = code;
        }
    }
}

void myinflate_init(decompression_state_t *s, zstream_t *stream) {
    memset(s, 0, sizeof(decompression_state_t));
    init_bit_stream(&s->bs, stream);

    s->window_size = WINDOW_SIZE;
    s->wsize = WINDOWHALF_SIZE;
    s->strstart = 0;

    myinflate_init_static_tables();
}

void myinflate_block_init(decompression_state_t *s) {
    int i;

    s->strstart = 0;
    for (i = 0; i < L_CODES; ++i) s->dyn_ltree[i].Freq = 0;
    for (i = 0; i < D_CODES; ++i) s->dyn_dtree[i].Freq = 0;
    for (i = 0; i < BL_CODES; ++i) s->bl_tree[i].Freq = 0;

    memset(s->dyn_ltree, 0, sizeof(s->dyn_ltree));
    memset(s->dyn_ltree, 0, sizeof(s->dyn_dtree));
    memset(s->dyn_ltree, 0, sizeof(s->bl_tree));

    memset(s->rev_ltree, 0xff, sizeof(s->rev_ltree));
    memset(s->rev_dtree, 0xff, sizeof(s->rev_dtree));
    memset(s->rev_bltree, 0xff, sizeof(s->rev_bltree));

    s->dyn_ltree[END_BLOCK].Freq = 1;
}

void gen_codes(decompression_state_t *s, tree_desc_t *td) {
    int bits, code = 0, n;  
    int next_code[MAX_BITS+1];
    int *bl_count = s->bl_count;
    tree_element_t *tree = td->tree;

    for (bits = 1; bits <= MAX_BITS; ++bits) {
        next_code[bits] = code = (code + bl_count[bits - 1]) << 1;
    }
    for (n = 0; n <= td->max_code; ++n) {
        int len = tree[n].Len; 
        if  (len == 0) continue;
        tree[n].Code = bi_reverse(next_code[len]++, len);
        td->rev_tree[tree[n].Code] = n;
    }
}

/* repair bl tree */
void read_bl_tree(decompression_state_t *s, tree_desc_t *bldesc, int maxblindex) {
    int rank, bits;
    memset(s->bl_count, 0, sizeof(s->bl_count));
    for (rank = 0; rank < maxblindex; ++rank) {
        bits = get_bits(&s->bs, 3); 
        s->bl_tree[bl_order[rank]].Len = bits;
        s->bl_count[bits]++;
    }
    gen_codes(s, bldesc);
}

void read_tree(decompression_state_t *s, tree_desc_t *tdesc, tree_desc_t *bldesc) {
    int i, code, last_len = 0, curlen, count;
    int *rev_bltree  = bldesc->rev_tree;
    tree_element_t *tree = tdesc->tree;

    for (i = 0; i <= tdesc->max_code; ++i) {
        code = get_code(&s->bs, bldesc);
        switch (rev_bltree[code]) {
        case REP_3_6:
            count = get_bits(&s->bs, 2) + 3;
            while (count--) {
                tree[i++].Len = last_len;
            }
            i--;
            break;
        case REPZ_3_10:
            count = get_bits(&s->bs, 3) + 3;
            while (count--) {
                tree[i++].Len = 0;
            }
            i--;
            break;
        case REPZ_11_138:
            count = get_bits(&s->bs, 7) + 11;
            while (count--) {
                tree[i++].Len = 0;
            }
            i--;
            break;
        default:
            curlen = rev_bltree[code];
            tree[i].Len = curlen;
            last_len = curlen;
        };
    }
    gen_codes(s, tdesc);
}

int read_all_trees(decompression_state_t *s, 
            tree_desc_t *ltree_desc,
            tree_desc_t *dtree_desc,
            tree_desc_t *bltree_desc) 
{
    int rank, len;
    int lcodes, dcodes, blcodes, maxblindex;

    myinflate_block_init(s);
    s->strstart = 0;

    assert(get_bits(&s->bs, 3) == DYN_TREES);
    lcodes = get_bits(&s->bs, 5) + 257;
    dcodes = get_bits(&s->bs, 5) + 1;
    blcodes = get_bits(&s->bs, 4) + 4;
    maxblindex = get_bits(&s->bs, 5);

    ltree_desc->max_code = lcodes;
    dtree_desc->max_code = dcodes;
    bltree_desc->max_code = blcodes;

    read_bl_tree(s, bltree_desc, maxblindex);
    read_tree(s, ltree_desc, bltree_desc);
    read_tree(s, dtree_desc, bltree_desc);
}

int flush_block(decompression_state_t *s) {
    if (s->strstart)
        return output_bytes(s, s->window, s->strstart);
    return 0;
}

int output_bytes(decompression_state_t *s, char *p, int len) {
    zstream_t *stream = s->bs.stream;
    if (stream->avail_out < len) {
        return OUTSTREAM_NEED_REFILL;
    }
    memcpy(stream->next_out, p, len);
    stream->next_out += len;
    stream->avail_out -= len;
    return 0;
}

/* 1 - last one */
/* 0 - no last one */
int decode_block(decompression_state_t *s) {
    int code, lc, dist, extra, total_outbytes;;

    int *rev_ltree, *rev_dtree;

    tree_desc_t ltree_desc = { s->dyn_ltree, s->rev_ltree, 
        HEAP_SIZE, 0, &static_ldesc  };
    tree_desc_t dtree_desc = { s->dyn_dtree, s->rev_dtree, 
        2*D_CODES+1, 0, &static_ddesc  };
    tree_desc_t bltree_desc = { s->bl_tree, s->rev_bltree,
        2*BL_CODES+1, 0, &static_bldesc };

    rev_ltree = s->rev_ltree;
    rev_dtree = s->rev_dtree;

    total_outbytes = 0;
    s->strstart = 0;
    read_all_trees(s, &ltree_desc, &dtree_desc, &bltree_desc);

    while (1) {
        if (s->strstart >= s->wsize + MAX_DIST(s)) {
            assert(output_bytes(s, s->window, s->wsize) 
                    != OUTSTREAM_NEED_REFILL);
            total_outbytes += s->wsize;
            memcpy(s->window, s->window + s->wsize, s->wsize);
            s->strstart -= s->wsize;
        }

        code = get_code(&s->bs, &ltree_desc);
        code = rev_ltree[code];
        if (code >= LITERALS + 1) {
            code -= (LITERALS + 1);
            lc = base_length[code];
            extra = extra_lbits[code];
            if (extra) {
                lc += get_bits(&s->bs, extra);
            }

            dist = 0;
            code = get_code(&s->bs, &dtree_desc);
            code = rev_dtree[code];
            dist = base_dist[code];
            extra = extra_dbits[code];
            if (extra) {
                dist += get_bits(&s->bs, extra);
            }

            assert(lc >= MIN_MATCH && lc <= MAX_MATCH);
            assert(dist <= MAX_DIST(s));
            assert(s->strstart - dist >= 0);
            memmove(s->window + s->strstart, s->window + s->strstart - dist, lc);
        } else if (code != END_BLOCK) {
            s->window[s->strstart++] = lc;
        } else {
            /* END_BLOCK here */
            break;
        }
    }
    if (s->strstart) {
        assert(output_bytes(s, s->window, s->strstart) != OUTSTREAM_NEED_REFILL);
        total_outbytes += s->strstart;
    }
    return total_outbytes == BUFFER_SIZE;
}

int myinflate(decompression_state_t *s) {
    return decode_block(s);
}

const char compressed_buf[] = {
0x4d, 0xa6, 0x8c, 0x25, 0x95, 0x60, 0x58, 0x49, 0x82, 0x3a, 0xe9, 0x07, 0x41, 0x05, 0x45, 0x6e, 
0x25, 0x13, 0xd3, 0x55, 0x7e, 0x7e, 0x8b, 0xb6, 0x23, 0x0d, 0xe6, 0xb3, 0x81, 0xf0, 0x00, 0x35, 
0x11, 0xc4, 0x67, 0x83, 0x45, 0x8d, 0xae, 0xf2, 0x0a, 0x3f, 0x23, 0x08, 0x7f, 0x6e, 0x3b, 0x81, 
0xd4, 0xb2, 0x63, 0xed, 0x82, 0xee, 0x35, 0x21, 0x8b, 0xbd, 0xdb, 0x1d, 0xde, 0x37, 0xfe, 0x74, 
0x32, 0x2b, 0x6f, 0x4f, 0x55, 0x9a, 0x1a, 0xba, 0xac, 0x4b, 0xe9, 0xf5, 0x67, 0x4d, 0x8e, 0xa2, 
0x02, 0x13, 0x75, 0x8b, 0x22, 0x21, 0xe6, 0xab, 0x89, 0xa1, 0x83, 0x8e, 0x31, 0x5c, 0x25, 0x5b, 
0x94, 0xdd, 0x1e, 0x33, 0x45, 0x76, 0x3d, 0x06, 0xb5, 0x2a, 0x21, 0xc6, 0x07, 0xcf, 0x28, 0x16, 
0x13, 0xf2, 0x1b, 0x20, 0x17, 0x21, 0x0f, 0xc5, 0x27, 0x97, 0xf6, 0xa6, 0x91, 0x0d, 0xcc, 0x03, 
0x27, 0x01, 0x0d, 0xce, 0x05, 0x81, 0x4c, 0x71, 0xd6, 0x86, 0x16, 0xae, 0x97, 0x22, 0x5f, 0x16, 
0xe3, 0xe8, 0x02, 0x86, 0x34, 0x3a, 0x86, 0xcd, 0x79, 0x8b, 0x96, 0x15, 0x96, 0x33, 0x84, 0x57, 
0x10, 0xa5, 0xd0, 0x83, 0x43, 0x6f, 0xe8, 0x91, 0xea, 0x35, 0x6a, 0x37, 0x2a, 0x11, 0xd2, 0x17, 
0xde, 0xd7, 0xaf, 0xaf, 0x8d, 0xf6, 0x26, 0x20, 0x15, 0xc5, 0xd5, 0xc5, 0xaa, 0x84, 0x10, 0xd7, 
0xaf, 0xcd, 0x74, 0x3d, 0x6f, 0xf3, 0x92, 0xad, 0x08, 0x08, 0x4b, 0x28, 0x85, 0xab, 0xf9, 0x89, 
0x71, 0xa5, 0xd5, 0x0e, 0x35, 0x0e, 0x27, 0x85, 0xb8, 0xf7, 0x5b, 0x2f, 0x82, 0xc7, 0x86, 0xf6, 
0x27, 0x5b, 0x4b, 0x36, 0xd0, 0xa5, 0xc5, 0x1b, 0x4d, 0x65, 0x02, 0x35, 0xdf, 0x0a, 0x73, 0x3a, 
0x07, 0x89, 0x03, 0xde, 0x17, 0x22, 0x69, 0xcc, 0xf6, 0x13, 0x80, 0x1d, 0x52, 0x35, 0x52, 0xdf, 
0x42, 0x5f, 0xb3, 0xa7, 0x8f, 0x3f, 0xdb, 0x29, 0x13, 0x4e, 0x41, 0x97, 0xe9, 0x92, 0xb3, 0x9b, 
0x1f, 0x4d, 0xf3, 0x8e, 0xf7, 0xcd, 0xd9, 0x3d, 0x07, 0x60, 0xb9, 0xa4, 0x61, 0xb5, 0x1d, 0xca, 
0x94, 0xaa, 0xbf, 0x4b, 0xa8, 0xf1, 0x7a, 0xb7, 0x5c, 0x05, 0x29, 0xaf, 0xcf, 0x67, 0xce, 0x7d, 
0x12, 0xc5, 0xa1, 0xc2, 0x2b, 0x88, 0xdd, 0xb2, 0xb3, 0x03, 0x91, 0xc6, 0x65, 0x57, 0x05, 0x60, 
0x07, 0xff, 0x22, 0x76, 0x67, 0x8e, 0xaf, 0x2d, 0xb6, 0x4c, 0xff, 0x81, 0x88, 0xa1, 0xaa, 0x2b, 
0x55, 0x40, 0x68, 0xd4, 0xe9, 0xec, 0x6e, 0x73, 0x8d, 0x2d, 0x40, 0x9d, 0x01, 0x37, 0x6c, 0x0c, 
0xb2, 0xd8, 0x2a, 0xe8, 0x01, 0x8c, 0x65, 0x62, 0x94, 0x11, 0x67, 0x0f, 0x4e, 0x7a, 0xe9, 0x8b, 
0xad, 0x9b, 0xc1, 0x68, 0x0a, 0xc3, 0x8e, 0x85, 0x94, 0xa0, 0x4a, 0x13, 0x99, 0x4d, 0x87, 0x6a, 
0xa9, 0x15, 0xd9, 0x0f, 0x75, 0x03, 0x82, 0x93, 0x2e, 0x48, 0x38, 0x29, 0x97, 0xe3, 0x5f, 0x8b, 
0xed, 0x87, 0x01, 0x4a, 0x07, 0xf7, 0x6f, 0x3f, 0x2f, 0x9b, 0x6f, 0xb3, 0x5f, 0x16, 0x4f, 0x0f, 
0x0f, 0x0f, 0xaa, 0xaa, 0x10, 0xbb, 0x61, 0x99, 0x03, 0xba, 0x39, 0x3b, 0xa9, 0x30, 0xea, 0x0b, 
0x34, 0xd9, 0x4d, 0x4e, 0x85, 0x49, 0xcd, 0xab, 0x0f, 0xcf, 0xc9, 0x97, 0x41, 0xcf, 0xaa, 0x2f, 
0xde, 0x49, 0x3d, 0x37, 0x6f, 0x01, 0x8d, 0x01, 0x42, 0x71, 0x62, 0x80, 0x82, 0x99, 0x01, 0x37, 
0x23, 0x6d, 0xf3, 0xdd, 0x5a, 0xa4, 0x45, 0xaa, 0xb8, 0xe1, 0x54, 0x8c, 0xac, 0x79, 0xde, 0x11, 
0xc2, 0x9a, 0xb8, 0x25, 0xd4, 0x36, 0x1f, 0x61, 0x5d, 0x9c, 0x19, 0xb5, 0x83, 0xc3, 0x0a, 0x79, 
0x43, 0x4c, 0x1d, 0xca, 0x2d, 0x00, 0xb9, 0x8a, 0x0e, 0xdc, 0x9a, 0x41, 0x80, 0x58, 0x9c, 0x17, 
0x97, 0x0b, 0x1f, 0x09, 0xc1, 0xe4, 0x60, 0x89, 0x8d, 0x8d, 0xb2, 0xea, 0x5b, 0xfb, 0xbb, 0x2d, 
0x0d, 0x9f, 0x6b, 0x29, 0x12, 0x4c, 0x10, 0x06, 0x96, 0xa1, 0xa5, 0x81, 0x62, 0x9f, 0x51, 0xa6, 
0x95, 0xa8, 0x69, 0xd0, 0x96, 0x12, 0x73, 0x2b, 0x4b, 0xa1, 0x70, 0x27, 0x99, 0x52, 0xa5, 0x9f, 
0xfe, 0xaa, 0x49, 0x0a, 0xf0, 0x00, 0x92, 0xd4, 0x2b, 0x2e, 0xb4, 0x73, 0x17, 0x7f, 0x94, 0x23, 
0xd3, 0x09, 0x09, 0x81, 0xdb, 0x65, 0xbe, 0xab, 0xf4, 0x5c, 0x2b, 0xb7, 0xcd, 0x52, 0x0f, 0xde, 
0xb3, 0x39, 0x51, 0xef, 0xef, 0x29, 0x00, 0x2e, 0x45, 0xdb, 0x2a, 0x5e, 0x2a, 0xd5, 0xd1, 0x15, 
0x44, 0x8d, 0xc6, 0xb1, 0x88, 0x1b, 0xcc, 0x6f, 0x55, 0x21, 0xca, 0x03, 0x31, 0x57, 0x0d, 0xd9, 
0xb0, 0x60, 0xca, 0x21, 0x1d, 0x87, 0xc6, 0xb0, 0x1f, 0x80, 0x84, 0xf8, 0x3e, 0xc5, 0x87, 0x81, 
0x43, 0xba, 0xbb, 0x2f, 0xaf, 0x2f, 0xdf, 0xeb, 0x3d, 0x06, 0xfe, 0xb5, 0x69, 0x70, 0x11, 0xd1, 
0xcc, 0x8f, 0x32, 0x14, 0xff, 0x00, };

int main() {
    int i;
    char inbuf[1024*1024], outbuf[1024*1024];

    zstream_t stream;
    decompression_state_t dstate;

    stream.next_in = (char*)compressed_buf;
    stream.avail_in = sizeof(compressed_buf);
    stream.next_out = outbuf;
    stream.avail_out = sizeof(outbuf);

    myinflate_init(&dstate, &stream);
    assert(myinflate(&dstate) == 0);
    assert(stream.avail_in == 0);

    printf("=%d bytes read,\n=%d bytes written\n", 
            (int)sizeof(compressed_buf)-stream.avail_in, 
            (int)sizeof(outbuf)-stream.avail_out);
    return 0;
}

