#include "blockcache.h"

boost::thread_specific_ptr<ThreadVar> threadvalue;

BlockCache::BlockCache(int blocksize, long long use_mem)
    : can_be_used(1),
      block_size(blocksize),
      hash_link_freelist(NULL),
      block_link_freelist(NULL)
{
    blocks_num = calculate_optimal_blocksnum(use_mem);
    total_size = get_memory_needed(blocks_num);

    char *p = memroot = (char*)malloc(total_size);
    memset(p, 0, total_size);

    changed_block_root = (BlockLink**)p;
    changed_block_root_size = blocks_num;
    p += changed_block_root_size * sizeof(BlockLink *);

    for (int i = 0; i < changed_block_root_size; ++i) {
        changed_block_root[i] = NULL;
    }

    hash_root = (HashLink **)p;
    hash_root_size = blocks_num;
    p += hash_root_size * sizeof(HashLink *);

    for (int i = 0; i < hash_root_size; ++i) {
        hash_root[i] = NULL;
    }

    hash_links_unused_p = (HashLink **)p;
    hash_links_unused = blocks_num * 2;
    p += hash_links_unused * sizeof(HashLink *);

    for (int i = 0; i < hash_links_unused; ++i) {
        HashLink *hlink = hash_links_unused_p[i] = (HashLink*)p;
        hlink->block = NULL;
        p += sizeof(HashLink);
    }

    block_links_unused_p = (BlockLink **)p;
    block_links_unused = blocks_num;
    p +=  block_links_unused * sizeof(BlockLink *);

    for (int i = 0; i < block_links_unused; ++i) {
        BlockLink *blink = block_links_unused_p[i] = (BlockLink*)p;
        blink->hash_link = NULL;
        p += sizeof(BlockLink);
    }

    for (int i = 0; i < block_links_unused; ++i) {
        BlockLink *blink = block_links_unused_p[i];
        blink->buffer = p;
        p += block_size;
    }
    init_file_operations(&f_op, 1);
}

BlockCache::~BlockCache() {
    end_key_cache();
    free(memroot);
    memroot = NULL;
}

void
BlockCache::end_key_cache() { 
    boost::mutex::scoped_lock scoped_lock(gl_lock);

    can_be_used = 0;
    while (block_links_unused != blocks_num) {
        /* removing from lru */
        BlockLink *blink = lru.top();
        if (blink)
            free_block(blink);

        /* removing from hash_root */
        for (int i = 0; i < hash_root_size; ++i) {
            if (hash_root[i]) {
                HashLink *p = hash_root[i]->next;
                do {
                    if (p->block) {
                        free_block(p->block);
                    }
                    p = p->next;
                } while (p != hash_root[i]);
            }
        }
    }
}

long long
BlockCache::get_memory_needed(int blocks_num) const {
    int block_links_total = blocks_num;
    int hash_links_total = 2 * block_links_total;

    return 
        block_links_total * sizeof(BlockLink *) /* for changed block_root */
        + block_links_total * sizeof(HashLink *) /* for hash_root */
        + hash_links_total * sizeof(HashLink *)
        + hash_links_total * sizeof(HashLink)
        + block_links_total * sizeof(BlockLink *)
        + block_links_total * sizeof(BlockLink)
        + (long long)block_links_total * (long long)block_size;
}

int 
BlockCache::calculate_optimal_blocksnum(long long use_mem) const {
    const int min_block_size = 8;
    int lower_blocks_num = 1;
    int upper_blocks_num = use_mem / min_block_size;
    while (lower_blocks_num < upper_blocks_num) {
        if (upper_blocks_num - lower_blocks_num  == 1) {
            return lower_blocks_num;
        }
        int blocks_num_middle
            = lower_blocks_num + (upper_blocks_num - lower_blocks_num) / 2;
        if (get_memory_needed(blocks_num_middle) >= use_mem) { 
            upper_blocks_num = blocks_num_middle - 1;
        } else {
            lower_blocks_num = blocks_num_middle;
        }
    }
    return lower_blocks_num;
}

int 
BlockCache::get_hashlink_hsh(int fd, int offset) const {
    return (offset / block_size + fd) % hash_root_size;
}

int 
BlockCache::get_blocklink_hsh(int fd) const {
    return fd % changed_block_root_size;
}

/* must be called with locked gl_lock */
BlockLink*
BlockCache::find_block(int fd, int offset, int wrmode) {
    ssize_t err;
    ThreadVar *thr = get_thread_var();

restart:
    /* hashlink search */
    HashLink *hlink = hash_root[get_hashlink_hsh(fd, offset)];
    for (; hlink; hlink = hlink->next) {
        if (hlink->fd == fd && hlink->diskpos == offset) {
            break;
        }
    }
    if (!hlink) {
        if (hash_links_unused) {
            if (hash_link_freelist) {
                hlink = hash_link_freelist;      
                hash_link_freelist = hash_link_freelist->next;
            } else {
                hlink = *hash_links_unused_p++;
            }
            hlink->reset(1);
            --hash_links_unused;
        } else {
            /* waiting for hash-link */
            struct Page page;
            page.fd = fd;
            page.diskpos = offset;
            thr->opt_info = (void*)&page;
            waiting_for_hlink.link(thr);
            thr->suspend.wait(gl_lock);
            hlink = (HashLink *)thr->opt_info;
            hlink->reset(0);
        }
    }

    ASSERT(hlink);
    hlink->requests++;

    /* blocklink search */
    BlockLink *block = hlink->block;
    if (!block) {
        if (block_links_unused) {
            /* freelist, unused */
            if (block_link_freelist) {
                block = block_link_freelist;      
                block_link_freelist = block_link_freelist->next_used;
            } else {
                block = *block_links_unused_p++;
            }
            block->reset(1);
            --block_links_unused;
        } else {
            if (lru.empty()) {
                /* waiting for block from lru */
                thr->opt_info = (void*)hlink;
                waiting_for_blink.link(thr);
                thr->suspend.wait(gl_lock);
                block = (BlockLink *)thr->opt_info;
            } else {
                /* getting from lru directly */
                block = lru.pop();  
            }
            block->reset(0);
        }
    }

    ASSERT(block);
    block->requests++;

    /* from unused, first thread */
    if (!block->is_assigned()) {
        ASSERT(block->status == 0);
        block->hash_link = hlink;

        gl_lock.unlock();
        err = f_op.pread(fd, block->buffer, block_size, offset);
        gl_lock.lock();

        if (err == -1) {
            block->status |= BLOCK_ERROR;
        }

        block->status |= BLOCK_READ;
        block->wqueue[COND_FOR_REQUESTED].clear();
    } else 
        /* request from second and further threads */
    if (block->hash_link == hlink) {

        if (wrmode && block->status & BLOCK_IN_FLUSHWRITE) {
            block->wqueue[COND_FOR_SAVED].link(thr);
            thr->suspend.wait(gl_lock);
            goto restart; /* my block is in eviction */
        }

        if (block->status & BLOCK_REASSIGNED) {
            block->wqueue[COND_FOR_REQUESTED].link(thr);
            thr->suspend.wait(gl_lock);
            goto restart; 
        }

        if (block->status & BLOCK_READ) {
            return block;
        }

        /* waiting for read */
        block->wqueue[COND_FOR_REQUESTED].link(thr);
        thr->suspend.wait(gl_lock);
    } else 
    /* block->hash_link != hlink here (from lru) */
    {
        /* optimize here for readers */
        if (block->status & BLOCK_DIRTY) {
            /* is it first thread, that took block from lru */
            if (!(block->status & BLOCK_IN_FLUSHWRITE)) {
                block->status |= BLOCK_IN_FLUSHWRITE;

                gl_lock.unlock();
                err = f_op.pwrite(fd, block->buffer, block_size, offset);
                gl_lock.lock();

                if (err == -1) {
                    block->status |= BLOCK_ERROR;
                }

                unlink_changed(block);
                block->status &= ~(BLOCK_DIRTY | BLOCK_IN_FLUSHWRITE);
                block->wqueue[COND_FOR_SAVED].clear(); 
    
            } else {
                /* just wait for flush */
                block->wqueue[COND_FOR_SAVED].link(thr);
                thr->suspend.wait(gl_lock);
            }
        }

        if (block->status & BLOCK_ERROR)
            return block;

        ASSERT(!(block->status & (BLOCK_DIRTY | BLOCK_IN_FLUSHWRITE)));

        /* first thread to handle reassignment */
        if (!(block->status & BLOCK_REASSIGNED)) {
            block->status |= BLOCK_REASSIGNED;

            gl_lock.unlock();
            err = f_op.pread(fd, block->buffer, block_size, offset);
            gl_lock.lock();

            if (err == -1) {
                block->status |= BLOCK_ERROR;
            }

            block->status &= ~BLOCK_REASSIGNED;
            block->status |= BLOCK_READ;
            block->hash_link = hlink; 
            block->wqueue[COND_FOR_REQUESTED].clear();
        } else {
            block->wqueue[COND_FOR_REQUESTED].link(thr);
            thr->suspend.wait(gl_lock);
        }
        ASSERT(block->status & BLOCK_READ);
    }
    return block;
}

int 
BlockCache::read(int fd, char *buffer, int size, int offset) {
    boost::mutex::scoped_lock scoped_lock(gl_lock);

    int page_offset = offset % block_size;
    offset -= page_offset;

    if (!can_be_used)
        return 1;

    for (int i = 0; i < (size + block_size - 1)/ block_size; ++i) {
        BlockLink *blink = find_block(fd, offset, 0);

        if (blink->status & BLOCK_ERROR) {
            remove_reader(blink->hash_link);
            free_block(blink);
            return 1;
        } 

        memcpy(buffer, blink->buffer + page_offset, 
                block_size - page_offset);

        unreg_block_request(blink);
        remove_reader(blink->hash_link);

        buffer += block_size - page_offset;
        offset += block_size;
        page_offset = 0;
    }
    return 0;
}

int 
BlockCache::write(int fd, char *buffer, int size, int offset) {
    boost::mutex::scoped_lock scoped_lock(gl_lock);

    int page_offset = offset % block_size;
    offset -= page_offset;

    if (!can_be_used)
        return 1;

    for (int i = 0; i < (size + block_size - 1)/ block_size; ++i) {
        BlockLink *blink = find_block(fd, offset, 1);

        if (blink->status & BLOCK_ERROR) {
            remove_reader(blink->hash_link);
            free_block(blink);
            return 1;
        } 

        memcpy(blink->buffer + page_offset,
                buffer, block_size - page_offset);

        blink->offset = offset + page_offset;
        blink->status |= BLOCK_DIRTY;
        link_changed(blink);
        remove_reader(blink->hash_link);

        unreg_block_request(blink);

        buffer += block_size - page_offset;
        offset += block_size;
        page_offset = 0;
    }
    return 0;
}

int
BlockCache::flush(int fd) {
    boost::mutex::scoped_lock scoped_lock(gl_lock);

    if (!can_be_used)
        return 1;

    BlockLink *root = changed_block_root[get_blocklink_hsh(fd)];
    if (root) {
        /* reading from end to beginning, to allow concurrent access */
        BlockLink *last = root->prev_changed;
        while (last != root) {
            flush_block(last);
            last = last->prev_changed;
        }
    }
    return 0;
}

void 
BlockCache::flush_block(BlockLink *blink) {
    ssize_t err;
    ASSERT(blink->status & BLOCK_DIRTY);

    /* don't touch blocks in eviction, they'll handle it themselves */
    if (!(blink->status & BLOCK_IN_EVICTION)) {
        blink->status |= BLOCK_IN_FLUSHWRITE;
        blink->requests++;

        gl_lock.unlock();
        err = f_op.pwrite(blink->hash_link->fd, 
                blink->buffer, block_size, blink->offset);
        gl_lock.lock();

        if (err == -1) {
            blink->status |= BLOCK_ERROR;
        }

        unreg_block_request(blink);
        unlink_changed(blink);
        blink->status &= ~(BLOCK_DIRTY | BLOCK_IN_FLUSHWRITE);
        blink->wqueue[COND_FOR_SAVED].clear(); 
    }
}

void 
BlockCache::free_block(BlockLink *blink) {
    if (!(blink->status & BLOCK_IN_EVICTION)) {
        wait_for_readers(blink);

        if (!(blink->status & BLOCK_ERROR)
                && blink->status & BLOCK_DIRTY)
        { flush_block(blink); }

        /* unlink hash */
        blink->hash_link->block = NULL;
        blink->hash_link->opt_info = NULL;
        unlink_hash(blink->hash_link);

        /* if it is in lru, unlink */
        if (blink->next_changed) {
            lru.unlink(blink);
        }

        /* move blink to freelist */
        blink->next_used = block_link_freelist;
        block_link_freelist = blink;
        block_links_unused++;

    }
}

void
BlockCache::wait_for_readers(BlockLink *blink) {
    ASSERT(blink->hash_link);
    ThreadVar *thr = get_thread_var();
    while (blink->hash_link->requests > 0) {
        blink->hash_link->opt_info = (ThreadVar *)thr;
        thr->suspend.wait(gl_lock);
    }
}

void
BlockCache::unreg_block_request(BlockLink *blink) {
    ASSERT(blink->requests);
    if (!--blink->requests) {
        blink->requests = 0;
        int num_waked = 0;
        if (blink->temperature == BLOCK_WARM
                && !waiting_for_blink.empty())
        {
            ThreadVar *last = waiting_for_blink.top();
            HashLink *first_link = (HashLink*)last->opt_info;
            for (; last; last = last->next) {
                HashLink *link = (HashLink*)last->opt_info;
                if (first_link == link) {
                    last->opt_info = (void *)blink;
                    last->suspend.notify_one();    
                    num_waked++;
                }
            }
        }
        if (!num_waked)
            lru.link(blink);
    }
}

void
BlockCache::remove_reader(HashLink *hlink) {
    ASSERT(hlink->requests > 0);
    if (--hlink->requests)
        return;
    unlink_hash(hlink);
}

void
BlockCache::unlink_hash(HashLink *hlink) {
    /* remove reader here */
    if (hlink->opt_info) {
        ThreadVar *thr = (ThreadVar *)hlink->opt_info;
        thr->suspend.notify_one();
        return;
    }

    int num_waked = 0;
    if (!waiting_for_hlink.empty()) {
        ThreadVar *last = waiting_for_hlink.top();
        Page *first_page = (Page *)last->opt_info;
        for (; last; last = last->next) {
            Page *page = (Page*)last->opt_info;
            if (page->fd == first_page->fd  
                && page->diskpos == first_page->diskpos) 
            {
                last->opt_info = (void *)hlink;
                last->suspend.notify_one();    
                num_waked++;
            }
        }
    }
    if (!num_waked) {
        hlink->next = hash_link_freelist;
        hash_link_freelist = hlink;
        hash_links_unused++;
    }
}

void
BlockCache::link_changed(BlockLink *blink) {
    ASSERT(!blink->prev_changed && !blink->next_changed);
    ASSERT(blink->hash_link);
    BlockLink **root = &changed_block_root[
        get_blocklink_hsh(blink->hash_link->fd)];

    blink->next_changed = *root;
    if (*root) {
        blink->prev_changed = (*root)->prev_changed;
        (*root)->prev_changed = blink;
    }
    *root = blink;
}

void
BlockCache::unlink_changed(BlockLink *blink) {
    BlockLink *prev = blink->prev_changed;
    if (prev)
        prev->next_changed = blink->next_changed;
    if (blink->next_changed)
        blink->next_changed->prev_changed = prev;
}

int
BlockCache::current_time() {
    boost::posix_time::time_duration curr_clock
        = boost::posix_time::microsec_clock::universal_time().time_of_day();
    return curr_clock.total_microseconds();
}

int 
BlockCache::timediff(int from, int to) {
    if (to < from) {
        return to + (86400 * 1000 - from);
    }
    return to - from;
}

void 
BlockLru::link(BlockLink *blink) {
    ASSERT(blink->requests == 0); 
    ASSERT(!blink->prev_used && !blink->next_used);

    blink->temperature = BLOCK_WARM;
    if (!begin) {
        begin = blink;
        begin->next_used = begin;
        begin->prev_used = begin;
    } else {
        blink->next_used = begin;
        blink->prev_used = begin->prev_used;
        begin->prev_used = blink;
        begin = blink;
    }
    ASSERT(blink->next_used && blink->prev_used);
}

void 
BlockLru::unlink(BlockLink *blink) {
    ASSERT(blink->prev_used && blink->next_used);
    if (blink == begin) {
        begin = NULL;
    } else {
        blink->prev_used->next_used = blink->next_used;
        blink->next_used->prev_used = blink->prev_used;
    }
    blink->prev_used = blink->next_used = NULL;
}

BlockLink *
BlockLru::pop() {
    if (!begin)
        return NULL;
    BlockLink *oldone = begin->prev_used;
    unlink(oldone);
    return oldone;
}

BlockLink *
BlockLru::top() {
    return begin->prev_used;
}

void 
BlockLruWithTemperature::link(BlockLink *blink) {
    ASSERT(blink);
    ASSERT(blink->requests == 0); 
    if (blink->temperature == BLOCK_COLD) {
        ASSERT(blink->hits_left == BlockLink::INIT_HITS_LEFT);
        blink->temperature = BLOCK_WARM;
    } else if (blink->temperature = BLOCK_WARM) {
        if (!blink->hits_left) {
            blink->temperature = BLOCK_HOT;
        } else {
            --blink->hits_left;
        }
    } else if (blink->temperature == BLOCK_HOT) {
        if (BlockCache::timediff(blink->last_hit_time, 
                BlockCache::current_time()) 
            > BlockLink::PROTOMOTION_TO_WARM)
        {
            blink->temperature = BLOCK_WARM;    
            blink->hits_left = BlockLink::INIT_HITS_LEFT;
        }
    }

    BlockLink **ins_last = blink->temperature == BLOCK_HOT
        ? &hot_last : &warm_last;

    if (!*ins_last) {
        *ins_last = blink;
        blink->prev_used = *ins_last;
        blink->next_used = *ins_last;
    } else {
        blink->prev_used = *ins_last;
        blink->next_used = (*ins_last)->next_used;
        (*ins_last)->next_used = blink;
        *ins_last = blink;
    }
    blink->last_hit_time = BlockCache::current_time();
}

void 
BlockLruWithTemperature::unlink(BlockLink *blink) {
    ASSERT(blink);
    BlockLink **ins_last = blink->temperature == BLOCK_HOT
        ? &hot_last : &warm_last;
    if (blink == *ins_last) {
        if ((*ins_last)->prev_used != *ins_last)
            *ins_last = NULL;
        else
            *ins_last = blink->prev_used;
    } else {
        blink->prev_used->next_used = blink->next_used;
        blink->next_used->prev_used = blink->prev_used;
    }
    blink->prev_used = blink->next_used = NULL;
}

BlockLink *
BlockLruWithTemperature::pop() {
    BlockLink *blink;
    if (warm_last) {
        blink = warm_last->next_used;
        unlink(blink);
        return blink;
    }
    if (hot_last) {
        blink = hot_last->next_used;
        unlink(blink);
        return blink;
    }
    return NULL;
}

BlockLink *
BlockLruWithTemperature::top() {
    if (!warm_last)
        return hot_last->next_used;
    return warm_last->next_used;
}

void
ThreadWaitQueue::link(ThreadVar *thr) {
    thr->prev = NULL;
    thr->next = last_thread;
    if (last_thread)
        last_thread->prev = thr;
    last_thread = thr;
}

void
ThreadWaitQueue::unlink(ThreadVar *thr) {
    if (thr->prev)
        thr->prev->next = thr->next;
    if (thr->next)
        thr->next->prev = thr->prev;
    if (last_thread == thr)
        last_thread = NULL;
}

void
ThreadWaitQueue::clear() {
    ThreadVar *last = last_thread;
    for (; last; last = last->next) {
        last->suspend.notify_one();
    }
}

ThreadVar *
get_thread_var() { 
    return static_cast<ThreadVar*>(threadvalue.get()); 
}

void
init_threadvalue() {
    threadvalue.reset(new ThreadVar());
}
