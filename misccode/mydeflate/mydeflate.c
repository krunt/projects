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

#define LENGTH_CODES 29
#define DIST_CODE_LEN 512

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

#define d_code(dist) \
    ((dist) < 256 ? _dist_code[dist] : _dist_code[256+((dist)>>7)])

#define UPDATE_HASH(s,h,c) \
    (h = (((h)<<s->hash_shift) ^ (c)) & s->hash_mask)

#define INSERT_STRING(s,str,match_head) \
   (UPDATE_HASH(s,s->ins_h,s->window[(str) + MIN_MATCH-1]),\
    ((match_head) = (s)->head[(s)->ins_h]),\
    assert(s->ins_h >= 0 && s->ins_h < s->hash_size),\
    assert(str >= 0 && str < WINDOW_SIZE), \
    (s)->head[s->ins_h] = (str))

#define BLOCK_ADD_LIT(s,c,need_flush) \
    assert((c) < LITERALS); \
    assert((s)->last_bpos >= 0 && ((s)->last_bpos < s->buf_size));\
    (s)->l_buf[(s)->last_bpos] = (c); \
    (s)->d_buf[(s)->last_bpos++] = 0; \
    (s)->dyn_ltree[((unsigned char)(c))].Freq++; \
    (need_flush) = ((s)->last_bpos == (s)->buf_size);

#define BLOCK_ADD_DIST(s,dist,len,need_flush) \
    assert(len <= MAX_MATCH); \
    assert(dist > 0); \
    (s)->l_buf[(s)->last_bpos] = (len); \
    (s)->d_buf[(s)->last_bpos++] = (dist); \
    (s)->dyn_ltree[_length_code[len]+LITERALS+1].Freq++; \
    (s)->dyn_dtree[d_code(dist)].Freq++; \
    (need_flush) = ((s)->last_bpos == (s)->buf_size);

#define smaller(tree, n, m) (tree[n].Freq < tree[m].Freq)
#define SMALLEST 1

#define pqremove(s, tree, top) \
{\
    top = s->heap[SMALLEST]; \
    s->heap[SMALLEST] = s->heap[s->heap_len--]; \
    pqdownheap(s, tree, SMALLEST); \
}
    
/* flushing pending bytes and bits */
#define FLUSHBYTES(s) flush_pending(s, 0)
#define FLUSHALL(s) flush_pending(s, 1)

typedef struct static_tree_desc_s {
    int *extra_bits;
    int extra_base;
    int elems;
    int max_length;
} static_tree_desc_t;

typedef struct tree_desc_s {
    tree_element_t *tree;
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

typedef struct compression_state_s {
    char window[WINDOW_SIZE];

    int window_size;
    int wsize;

    int strstart;
    int lookahead;

    int ins_h; /* tmp hash value */

    int hash_bits;
    int hash_size;
    int hash_mask;
    int hash_shift;
    int *head; /* hash heads */

    int match_length;
    int match_start;

    int buf_size;
    char *l_buf; 
    char *d_buf;

    int last_bpos;
    int max_code;

    tree_element_t dyn_ltree[HEAP_SIZE];
    tree_element_t dyn_dtree[2*D_CODES+1];
    tree_element_t bl_tree[2*BL_CODES+1];

    int heap[HEAP_SIZE];
    int heap_len;
    int heap_max;
    int bl_count[MAX_BITS+1];

    char pending_buf[PENDING_BUF_SIZE];
    int pending_buf_size;
    int pending_buf_pointer;
    char pending_val;
    int bits_avail;

    /* length of block to flush */
    int opt_len; 

    zstream_t *stream;
} compression_state_t;

#define checkit(s) 
/*
#define checkit(s) do{\
    int i;\
    for (i = 0; i < s->hash_size; ++i) {\
        if (!(s->head[i] == NIL || (s->head[i] >= 0 && s->head[i] < WINDOW_SIZE)))\
        { printf("=%d\n", s->head[i]); assert(0); }\
    }} while(0)
*/


void pqdownheap(compression_state_t *s, tree_element_t *tree, int k)
{
    int v = s->heap[k];
    int j = k << 1;
    while (j <= s->heap_len) {
        if (j < s->heap_len && smaller(tree, s->heap[j+1], s->heap[j])) {
            j++; 
        }
        if (smaller(tree, v, s->heap[j])) break;

        s->heap[k] = s->heap[j]; 
        k = j;

        j <<= 1;
    }
    s->heap[k] = v;
}

int bi_reverse(int code, int len)
{
    int res = 0;
    do {
        res |= code & 1;
        code >>= 1, res <<= 1;
    } while (--len > 0);
    return res >> 1;
}

void gen_bitlen(compression_state_t *s, tree_desc_t *td)
{
    int bits, xbits, h, n, f, i, m;
    tree_element_t *tree = td->tree;
    int *extra = td->sdesc->extra_bits;
    int base = td->sdesc->extra_base;
    int max_length = td->sdesc->max_length;
    int overflow = 0;

    for (bits = 0; bits <= MAX_BITS; ++bits) s->bl_count[bits] = 0;
        
    tree[s->heap[s->heap_max]].Len = 0;
    for (h = s->heap_max+1; h < HEAP_SIZE; ++h) {
        n = s->heap[h]; 

        /*
        if (tree[n].Dad) {
printf("%d\n", n);
    for (i = s->heap_max + 1; i < HEAP_SIZE; ++i) {
        h = s->heap[i];
        printf("%d\n", tree[h].Dad);
    }
    printf("\n");
    assert(0);
        }
        assert(tree[n].Dad != NIL);
        */

        bits = tree[tree[n].Dad].Len + 1;
        if (bits > max_length) bits = max_length, overflow++;
        tree[n].Len = bits;

        if (n > td->max_code) continue; /* not a leaf node */

        assert(bits <= MAX_BITS); /* maybe no overflows ?) */
        s->bl_count[bits]++;

        xbits = 0;
        if (n >= base) xbits = extra[n - base];
        f = tree[n].Freq;
        s->opt_len += f * (bits + xbits);
    }
    if (overflow == 0) return;

    do {
        bits = max_length-1;
        while (s->bl_count[bits] == 0) bits--;
        s->bl_count[bits]--;      /* move one leaf down the tree */
        s->bl_count[bits+1] += 2; /* move one overflow item as its brother */
        s->bl_count[max_length]--;
        /* The brother of the overflow item also moves one step up,
         * but this does not affect bl_count[max_length]
         */
        overflow -= 2;
    } while (overflow > 0);

    for (bits = max_length; bits != 0; bits--) {
        n = s->bl_count[bits];
        while (n != 0) {
            m = s->heap[--h];
            if (m > td->max_code) continue;
            if ((unsigned) tree[m].Len != (unsigned) bits) {
                s->opt_len += ((long)bits - (long)tree[m].Len)
                              *(long)tree[m].Freq;
                tree[m].Len = bits;
            }
            n--;
        }
    }
}

void gen_codes(compression_state_t *s, tree_desc_t *td) {
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
    }
}

void mydeflate_init_static_tables() 
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

void init_block(compression_state_t *s) {
    int i;
    for (i = 0; i < L_CODES; ++i) s->dyn_ltree[i].Freq = 0;
    for (i = 0; i < D_CODES; ++i) s->dyn_dtree[i].Freq = 0;
    for (i = 0; i < BL_CODES; ++i) s->bl_tree[i].Freq = 0;

    memset(s->dyn_ltree, 0, sizeof(s->dyn_ltree));
    memset(s->dyn_ltree, 0, sizeof(s->dyn_dtree));
    memset(s->dyn_ltree, 0, sizeof(s->bl_tree));

    s->dyn_ltree[END_BLOCK].Freq = 1;
    s->opt_len = 0;
    s->last_bpos = 0;
}

void mydeflate_init(compression_state_t *state, zstream_t *stream) 
{
    int i;
    memset(state, 0, sizeof(compression_state_t));

    state->window_size = WINDOW_SIZE;
    state->wsize = WINDOWHALF_SIZE;
    state->strstart = 0;
    state->lookahead = 0;
    state->ins_h = 0;

    state->hash_bits = HASH_BITS;
    state->hash_size = 1<<state->hash_bits;
    state->hash_mask = state->hash_size - 1;
    state->hash_shift = ((state->hash_bits + MIN_MATCH-1)/MIN_MATCH);

    state->head = (int *)malloc(state->hash_size * sizeof(int));
    for (i = 0; i < state->hash_size; ++i) state->head[i] = NIL;

    state->match_length = 0;
    state->match_start = 0;

    state->buf_size = BUFFER_SIZE;
    state->l_buf = (char*)malloc(state->buf_size * sizeof(char));
    state->d_buf = (char*)malloc(state->buf_size * sizeof(char));
    state->max_code = 0;

    init_block(state);

    state->heap_len = 0;
    state->heap_max = 0;

    state->pending_buf_size = PENDING_BUF_SIZE;
    state->pending_buf_pointer = 0;
    state->pending_val = 0;
    state->bits_avail = 8;
    state->opt_len = 0;
    state->stream = stream;

    mydeflate_init_static_tables();
}

#define NEED_MORE_INPUT -1
#define NEED_MORE_OUTPUT -2
#define DEFLATE_OK 0

int determine_max_match(compression_state_t *s) 
{
    assert(s->match_start < s->strstart);
    assert(s->lookahead >= MIN_MATCH);

    char *pmatch = s->window + s->match_start;
    char *pstart = s->window + s->strstart;
    char *pend = pstart + MAX_MATCH;

    if (pmatch[0] != pstart[0] || pmatch[1] != pstart[1])
        return 0;

    pmatch += 2; pstart += 2;
    while (pstart < pend && *pmatch++ == *pstart++);
    return pmatch - s->window - s->match_start;
}

/* relax here */
void flush_pending(compression_state_t *s, int flush_bitbuf) {
    zstream_t *stream = s->stream;
    assert(stream->avail_out >= s->pending_buf_pointer + flush_bitbuf ? 1 : 0);
    if (s->pending_buf_pointer) {
        memcpy(stream->next_out, s->pending_buf, s->pending_buf_pointer);
        stream->next_out += s->pending_buf_pointer;
        stream->avail_out -= s->pending_buf_pointer;
        s->pending_buf_pointer = 0;
    }
    if (flush_bitbuf) {
        stream->next_out[0] = s->pending_val << (s->bits_avail);
        stream->avail_out--;
        stream->next_out++;
    }
}

int send_bits(compression_state_t *s, unsigned int num, int bits) 
{
    if (!bits)
        return 0;

    assert(bits > 0 && bits <= 32);
    assert(s->bits_avail > 0);
    if (bits >= s->bits_avail) {
        s->pending_val = (s->pending_val << s->bits_avail) 
            | (num & ((1 << s->bits_avail) - 1));
        s->pending_buf[s->pending_buf_pointer++] = s->pending_val;
        if (s->pending_buf_pointer == s->pending_buf_size) {
            FLUSHBYTES(s);
        }
        num >>= s->bits_avail;
        bits -= s->bits_avail;
        s->pending_val = 0;
        s->bits_avail = 8;
    }

    while (bits >= 8) {
        s->pending_buf[s->pending_buf_pointer++] = num & 255;
        if (s->pending_buf_pointer == s->pending_buf_size) {
            FLUSHBYTES(s);
        }
        num >>= 8;
        bits -= 8;
    }
    
    assert(bits >= 0);
    assert(bits < s->bits_avail);
    if (bits) {
        s->pending_val = (s->pending_val << bits) 
            | (num & ((1 << bits) - 1));
        s->bits_avail -= bits;
    }
    return 0;
}

void send_code(compression_state_t *s, int code, tree_element_t *tree) {
    assert(tree[code].Len >= 0 && tree[code].Len <= MAX_BITS);
    send_bits(s, tree[code].Code, tree[code].Len); 
}

int compress_block(compression_state_t *s,
        tree_desc_t *ltree, tree_desc_t *dtree)
{
    int i,j;
    int lc, dist, code, extra;
    for (i = 0; i < s->last_bpos; ++i) {
        lc = (int)((unsigned char)s->l_buf[i]);
        dist = (int)((unsigned char)s->d_buf[i]);

        if (dist == 0) {
            send_code(s, lc, ltree->tree);
        } else {
            code = _length_code[lc];
            send_code(s, code+LITERALS+1, ltree->tree);
            extra = extra_lbits[code];
            if (extra) {
                lc -= base_length[code];
                send_bits(s, lc, extra);
            }
            dist--;

            assert(dist >= 0 && dist < DIST_CODE_LEN);
            code = d_code(dist);

            assert(code < D_CODES);

            send_code(s, code, dtree->tree);
            extra = extra_dbits[code];
            if (extra) {
                dist -= base_dist[code];
                send_bits(s, dist, extra);
            }
        }
    }
    send_code(s, END_BLOCK, ltree->tree);
}

int build_tree(compression_state_t *s, tree_desc_t *td) 
{
    int i, heaplen = 0, max_code = 0, least;
    int n, m, node = 0;
    int elems = td->sdesc->elems;
    tree_element_t *tree = td->tree;
    assert(HEAP_SIZE >= elems);
    assert(elems > 0);
    checkit(s);
    for (i = 0; i < td->len; ++i) {
        if (tree[i].Freq) {
            s->heap[++heaplen] = max_code = i;
        } else {
            tree[i].Len = 0;
        }
        tree[i].Dad = NIL;
    }
    td->max_code = max_code;
    s->heap_len = heaplen;
    s->heap_max = HEAP_SIZE;

    for (i = heaplen / 2; i >= 1; --i) pqdownheap(s, tree, i);

    node = elems;
    do {
        pqremove(s, tree, n);
        m = s->heap[SMALLEST];

        tree[n].Dad = node;
        tree[m].Dad = node;
        tree[node].Freq = tree[n].Freq + tree[m].Freq;

        s->heap[--s->heap_max] = n;
        s->heap[--s->heap_max] = m;

        s->heap[SMALLEST] = node++;
        pqdownheap(s, tree, SMALLEST);
    } while (s->heap_len > 1);
    s->heap[--s->heap_max] = s->heap[SMALLEST];

    gen_bitlen(s, td);
    gen_codes(s, td);

    return 0;
}

void scan_tree(compression_state_t *s, tree_desc_t *tdesc) {
    int n;
    tree_element_t *tree = tdesc->tree;
    int count = 0;
    int min_count = 4, max_count = 7;
    int curlen, nextlen = tree[0].Len;

    if (nextlen == 0) max_count = 138, min_count = 3;
    tree[tdesc->max_code+1].Len = 0xffff;

    for (n = 0; n <= tdesc->max_code; ++n) {
        curlen = nextlen; nextlen = tree[n+1].Len;
        if (++count < max_count && curlen == nextlen) {
            continue;
        } else if (count < min_count) {
            s->bl_tree[curlen].Freq += count;
        } else if (curlen) {
            if (curlen != nextlen) s->bl_tree[curlen].Freq++;
            s->bl_tree[REP_3_6].Freq++;
        } else if (count <= 10) {
            s->bl_tree[REPZ_3_10].Freq++;
        } else {
            s->bl_tree[REPZ_11_138].Freq++;
        }
        count = 0; nextlen = curlen;
        if (nextlen == 0) {
            max_count = 138, min_count = 3;
        } else if (curlen == nextlen) {
            max_count = 6, min_count = 3;
        } else {
            max_count = 7, min_count = 3;
        }
    }
}

int build_bl_tree(compression_state_t *s, tree_desc_t *ldesc,
        tree_desc_t *ddesc, tree_desc_t *bldesc)
{
    int max_blindex;

    scan_tree(s, ldesc); 
    scan_tree(s, ddesc); 

    build_tree(s, bldesc);
    for (max_blindex = BL_CODES - 1; max_blindex >= 3; --max_blindex) {
        if (s->bl_tree[bl_order[max_blindex]].Len != 0) break;
    }
    return max_blindex;
}

void send_tree(compression_state_t *s, tree_desc_t *tdesc) {
    int i;
    tree_element_t *tree = tdesc->tree;
    int prevlen = -1;
    int count = 0;
    int curlen, nextlen = tree[0].Len;
    int min_count = 4, max_count = 7;

    if (nextlen == 0) max_count = 138, min_count = 3;

    for (i = 0; i <= tdesc->max_code; ++i) {
        curlen = nextlen; nextlen = tree[i+1].Len;
        if (++count < max_count && curlen == nextlen) {
            continue;
        } else if (count < min_count) {
            do { send_code(s, curlen, s->bl_tree); } while (--count != 0);
        } else if (curlen) {
            if (curlen != prevlen) {
                send_code(s, curlen, s->bl_tree); 
                count--;
            }
            assert(count >= 3 && count <= 6);
            send_code(s, REP_3_6, s->bl_tree);
            send_bits(s, count - 3, 2);
        } else if (count <= 10) {
            send_code(s, REPZ_3_10, s->bl_tree);
            send_bits(s, count - 3, 3);
        } else {
            send_code(s, REPZ_11_138, s->bl_tree);
            send_bits(s, count - 11, 7);
        }
        count = 0; prevlen = curlen;
        if (nextlen == 0) {
            max_count = 138, min_count = 3;
        } else if (curlen == nextlen) {
            max_count = 6, min_count = 3;
        } else {
            max_count = 7, min_count = 4;
        }
    }
}

void send_all_trees(compression_state_t *s, tree_desc_t *ldesc, 
        tree_desc_t *ddesc, tree_desc_t *bldesc, int max_blindex)
{
    int rank = 0;
    int lcodes = ldesc->max_code;
    int dcodes = ddesc->max_code;
    int blcodes = bldesc->max_code;

    send_bits(s, lcodes-257, 5);
    send_bits(s, dcodes-1, 5);
    send_bits(s, blcodes-4, 4);
    send_bits(s, max_blindex, 5);
    for (rank = 0; rank < max_blindex; ++rank) {
        send_bits(s, s->bl_tree[bl_order[rank]].Len, 3);
    }
    send_tree(s, ldesc);
    send_tree(s, ddesc);
}

int flush_block(compression_state_t *s) {
    int max_blindex;
    tree_desc_t ltree_desc = { s->dyn_ltree, HEAP_SIZE,   0, &static_ldesc  };
    tree_desc_t dtree_desc = { s->dyn_dtree, 2*D_CODES+1, 0, &static_ddesc  };
    tree_desc_t bltree_desc = { s->bl_tree, 2*BL_CODES+1, 0, &static_bldesc };

    build_tree(s, &ltree_desc);
    build_tree(s, &dtree_desc);

    max_blindex = build_bl_tree(s, &ltree_desc, &dtree_desc, &bltree_desc);

    s->opt_len += 3; /* send_bits */
    s->opt_len += 5 + 5 + 4 + max_blindex * 3; /* send all trees */
    s->opt_len += s->dyn_ltree[END_BLOCK].Len; /* compress-block */

    if ((s->opt_len + 7) / 8 > s->stream->avail_out)
        return NEED_MORE_OUTPUT;

    send_bits(s, DYN_TREES, 3);
    send_all_trees(s, &ltree_desc, &dtree_desc, &bltree_desc, max_blindex);
    compress_block(s, &ltree_desc, &dtree_desc);

    return 0;
}

int fill_window(compression_state_t *s) 
{
    int i;
    zstream_t *stream = s->stream;
    assert(s->lookahead < MIN_LOOKAHEAD);
    int more = s->window_size - s->strstart - s->lookahead;

    /* need to free some space */
    if (s->strstart >= s->wsize + MAX_DIST(s)) {
        memcpy(s->window, s->window + s->wsize, s->wsize);
        for (i = 0; i < s->hash_size; ++i) {
            s->head[i] = s->head[i] != NIL && s->head[i] > s->wsize 
                ? s->head[i] - s->wsize : NIL;
            assert(s->head[i] >= 0 && s->head[i] < WINDOW_SIZE);
        }
        s->strstart -= s->wsize;
        more += s->wsize;
    }

    if (stream->avail_in < (MIN_LOOKAHEAD - s->lookahead)) {
        return NEED_MORE_INPUT;
    }

    more = MIN(more, stream->avail_in);
    memcpy(s->window + s->strstart + s->lookahead,
            stream->next_in, more);
    s->lookahead += more;
    stream->avail_in -= more; 
    stream->next_in += more;

    assert(s->lookahead >= MIN_MATCH);
    s->ins_h = s->window[s->strstart];
    UPDATE_HASH(s,s->ins_h,s->window[s->strstart+1]);
    return more;
}

int mydeflate(compression_state_t *s, int finalize) 
{
    int i;
    int prev_head = 0, need_flush, result = 0;
    zstream_t *stream = s->stream;
    checkit(s);
    while (1) {
        if (s->lookahead < MIN_LOOKAHEAD) {
            if ((result = fill_window(s)) == NEED_MORE_INPUT)
                break;
        }

        assert(s->lookahead >= MIN_LOOKAHEAD);
        assert(s->strstart < s->window_size);
        prev_head = NIL;

        checkit(s);
        INSERT_STRING(s,s->strstart,prev_head);
        checkit(s);

        s->match_length = 0;
        if (prev_head != NIL && s->strstart - prev_head <= MAX_DIST(s)) {
            assert(s->strstart > prev_head);
            s->match_start = prev_head;
            s->match_length = determine_max_match(s); 
        }

        checkit(s);
        if (s->match_length >= MIN_MATCH) {
            assert(s->strstart > s->match_start);
            assert(s->match_length > 0);
            BLOCK_ADD_DIST(s, s->strstart - s->match_start, 
                    s->match_length, need_flush);

            s->strstart += s->match_length;
            s->lookahead -= s->match_length;

            s->ins_h = s->window[s->strstart];
            UPDATE_HASH(s,s->ins_h,s->window[s->strstart+1]);
            checkit(s);
        } else {
            checkit(s);
            BLOCK_ADD_LIT(s, s->window[s->strstart], need_flush);
            checkit(s);
            s->strstart++;
            s->lookahead--;
            checkit(s);
        }

        if (need_flush) {
            if ((result = flush_block(s)) == NEED_MORE_OUTPUT) { 
                break;
            }
            init_block(s);
        }
    }
    if (result == NEED_MORE_OUTPUT) {
        return result;
    }
    if (finalize) {
        assert(result == NEED_MORE_INPUT);
        flush_block(s);
        FLUSHALL(s);
        init_block(s);
        return DEFLATE_OK;
    }
    return result;
}

/* from memory
int main() {
    int i;
    char inbuf[1024*1024], outbuf[1024*1024];

    zstream_t stream;
    compression_state_t cstate;

    for (i = 0; i < sizeof(inbuf); ++i) {
        inbuf[i] = rand() & 255;
    }

    stream.next_in = inbuf;
    stream.avail_in = sizeof(inbuf);
    stream.next_out = outbuf;
    stream.avail_out = sizeof(outbuf);

    mydeflate_init(&cstate, &stream);
    assert(mydeflate(&cstate, 1) == DEFLATE_OK);
    assert(stream.avail_in == 0);

    printf("=%d bytes read,\n=%d bytes written\n", 
            (int)sizeof(inbuf)-stream.avail_in, 
            (int)sizeof(outbuf)-stream.avail_out);

    return 0;
}
*/

/*
int main(int argc, char **argv) {
    int i;
    FILE *fdin, *fdout;
    char inbuf[4096], outbuf[BUFFER_SIZE*2];
    int bytes_read;
    zstream_t stream;
    compression_state_t cstate;
    int total_bytes_read, total_bytes_written;

    if (argc != 3) {
        fprintf(stderr, "Usage %s: <infile> <outfile>\n", argv[0]);
        return -1;
    }

    fdin = fopen(argv[1], "rb");
    fdout = fopen(argv[2], "wb");
    if (!fdin || !fdout) {
        return -2;
    }

    mydeflate_init(&cstate, &stream);
    while ((bytes_read = fread(inbuf, 1, sizeof(inbuf), fdin)) == sizeof(inbuf)) {
        stream.next_in = inbuf;
        stream.avail_in = sizeof(inbuf);
        stream.next_out = outbuf;
        stream.avail_out = sizeof(outbuf);

        do {
            assert(mydeflate(&cstate, 0) == NEED_MORE_INPUT);
        } while (stream.avail_in != 0);

        if ((int)sizeof(outbuf) - stream.avail_out > 0) {
            fwrite(outbuf, 1, (int)sizeof(outbuf) - stream.avail_out, fdout);
        }

        total_bytes_read += bytes_read;
        total_bytes_written += (int)sizeof(outbuf) - stream.avail_out;
    }

    if (bytes_read > 0) {
        stream.next_in = inbuf;
        stream.avail_in = bytes_read;
        stream.next_out = outbuf;
        stream.avail_out = sizeof(outbuf);

        do {
            assert(mydeflate(&cstate, 1) == DEFLATE_OK);
        } while (stream.avail_in != 0);

        fwrite(outbuf, 1, (int)sizeof(outbuf) - stream.avail_out, fdout);

        total_bytes_read += bytes_read;
        total_bytes_written += (int)sizeof(outbuf) - stream.avail_out;
    }

    fclose(fdout);
    fclose(fdin);

    printf("=%d bytes read,\n=%d bytes written\n", 
            total_bytes_read, total_bytes_written);

    return 0;
}
*/


const char *etcpasswd = 
"root:x:0:0:root:/root:/bin/bash";

int main() {
    int i;
    char inbuf[1024*1024], outbuf[1024*1024];

    zstream_t stream;
    compression_state_t cstate;

    memcpy(inbuf, etcpasswd, strlen(etcpasswd));

    stream.next_in = inbuf;
    stream.avail_in = strlen(etcpasswd);
    stream.next_out = outbuf;
    stream.avail_out = sizeof(outbuf);

    mydeflate_init(&cstate, &stream);
    assert(mydeflate(&cstate, 1) == DEFLATE_OK);
    assert(stream.avail_in == 0);

    printf("{\n");
    for (i = 0; i < (int)sizeof(outbuf)-stream.avail_out; ++i) {
        if (i && !(i & 15)) {
            printf("\n");
        }
        printf("0x%02x, ", outbuf[i] & 255);
    }
    printf("}\n");

    printf("=%d bytes read,\n=%d bytes written\n", 
            (int)strlen(etcpasswd)-stream.avail_in, 
            (int)sizeof(outbuf)-stream.avail_out);

    return 0;
}
