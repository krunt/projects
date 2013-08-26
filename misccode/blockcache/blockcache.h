#ifndef BLOCKCACHE_DEF__
#define BLOCKCACHE_DEF__

#include <boost/thread/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/tss.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>
#include <time.h>
#include "myassert.h"
#include "file_operations.h"

struct ThreadVar {
    ThreadVar()
        : next(NULL), 
          prev(NULL), 
          opt_info(NULL)
    {}

    boost::condition suspend;
    ThreadVar *prev, *next;
    void *opt_info;
};

extern boost::thread_specific_ptr<ThreadVar> threadvalue;
ThreadVar *get_thread_var();
void init_threadvalue();

struct Page {
    int fd;
    size_t diskpos;
};

class ThreadWaitQueue {
public:
    ThreadWaitQueue()
        : last_thread(NULL)
    {}

    void link(ThreadVar *thr);
    void unlink(ThreadVar *thr);
    ThreadVar *top() const { return last_thread; }
    void clear();

    void reset() { last_thread = NULL; }
    bool empty() const { 
        return last_thread == NULL; 
    }

private:
    ThreadVar *last_thread;
};

struct HashLink {
    HashLink() { reset(1); }

    void reset(int all) {
        if (all) 
            block = NULL;
        prev = next = NULL;
        fd = 0;
        diskpos = 0;
        requests = 0;
        opt_info = NULL;
    }

    struct HashLink *prev, *next;
    struct BlockLink *block;
    int fd;
    size_t diskpos;
    int requests;
    void *opt_info;

};

enum Temperature {
    BLOCK_COLD,  
    BLOCK_WARM,
    BLOCK_HOT,
};

enum StatusFlags {
    BLOCK_READ = 1,
    BLOCK_REASSIGNED = 2,
    BLOCK_DIRTY = 4,
    BLOCK_IN_FLUSHWRITE = 8,
    BLOCK_ERROR = 16,
    BLOCK_IN_EVICTION = 32,
};

enum WaitQueues {
    COND_FOR_SAVED = 0,
    COND_FOR_REQUESTED = 1,
    COND_LAST = 2,
};

struct BlockLink {
    enum { 
        INIT_HITS_LEFT = 3, 
        PROTOMOTION_TO_WARM = 5000, /* microseconds */
    };

    BlockLink() { reset(1); }

    void reset(int all) {
        if (all) hash_link = NULL;
        prev_used = next_used = NULL;
        prev_changed = next_changed = NULL;
        requests = 0;
        offset = 0;
        status = 0;
        hits_left = INIT_HITS_LEFT;
        temperature = BLOCK_COLD;
        wqueue[0].reset();
        wqueue[1].reset();
    }

    bool is_assigned() const {
        return hash_link != NULL;
    }

    HashLink *hash_link;
    BlockLink *prev_used, *next_used;
    BlockLink *prev_changed, *next_changed;
    int requests;

    char *buffer;
    int offset;
    int status;
    int hits_left;
    time_t last_hit_time;

    enum Temperature temperature;
    ThreadWaitQueue wqueue[COND_LAST];
};

class BlockLru {
public:
    BlockLru()
        : begin(NULL)
    {}

    void link(BlockLink *blink);
    void unlink(BlockLink *blink);
    BlockLink *top();
    BlockLink *pop();

    bool empty() const { 
        return begin == NULL;
    }

private:
    BlockLink *begin;
};

class BlockLruWithTemperature {
public:
    BlockLruWithTemperature()
        : warm_last(NULL),
          hot_last(NULL)
    {}

    void link(BlockLink *blink);
    void unlink(BlockLink *blink);
    BlockLink *top();
    BlockLink *pop();

    bool empty() const { 
        return warm_last == NULL && hot_last == NULL;
    }

private:
    BlockLink *hot_last;
    BlockLink *warm_last;
};

class BlockCache {
public:
    BlockCache(int blocksize, long long use_mem);
    ~BlockCache();

    int read(int fd, char *buffer, int size, int offset);
    int write(int fd, char *buffer, int size, int offset);
    int flush(int fd);

    int blocksize() const { return block_size; }
    int blocks_count() const { return blocks_num; }
    
private:
    BlockLink* find_block(int fd, int offset, int wrmode);
    int get_hashlink_hsh(int fd, int offset) const;
    int get_blocklink_hsh(int fd) const;

    long long get_memory_needed(int blocks_num) const;
    int calculate_optimal_blocksnum(long long use_mem) const;

    void unreg_block_request(BlockLink *blink);
    void unlink_hash(HashLink *hlink);

    void link_changed(BlockLink *blink);
    void unlink_changed(BlockLink *blink);

    void flush_block(BlockLink *blink);
    void free_block(BlockLink *blink);

    void wait_for_readers(BlockLink *blink);
    void end_key_cache();

    void remove_reader(HashLink *hlink);

public:
    /* from start of day in microseconds */
    static int current_time(); 
    static int timediff(int from, int to);

private:
    int can_be_used;

    char *memroot;

    int blocks_num;
    int block_size;
    int total_size;
    HashLink **hash_root;
    int hash_root_size;

    BlockLink **changed_block_root;
    int changed_block_root_size;

    int hash_links_unused;
    HashLink *hash_link_freelist;
    HashLink **hash_links_unused_p;

    int block_links_unused;
    BlockLink *block_link_freelist;
    BlockLink **block_links_unused_p;

    BlockLru lru;

    ThreadWaitQueue waiting_for_hlink;
    ThreadWaitQueue waiting_for_blink;

    file_operations f_op;

    /* block-cache global lock */
    boost::mutex gl_lock;
};

#endif /* BLOCKCACHE_DEF__ */
