#include <iostream>
#include <vector>
#include <time.h>
#include <fstream>
#include <sstream>
#include <boost/thread/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/tss.hpp>

struct ThreadVar {
    ThreadVar()
        : next(NULL), 
          prev(NULL), 
          lock_type(0)
    {}

    void wait_on_lock(boost::mutex &wl, int locktype) {
        lock_type = 1 << locktype;
        suspend.wait(wl);
        lock_type = 0;
    }

    boost::condition suspend;
    ThreadVar *prev, *next;
    int lock_type;
};

class Logger {
public:
    Logger(const char *fname = "log.txt") 
        : fs(fname, std::ios_base::out)
    {}

    void logit(const std::string &message) {
        fs << time(NULL) 
            << "(" << boost::this_thread::get_id() << ")"
            << ": " << message << "\n";
        fs.flush();
    }

private:
    std::fstream fs;
    boost::mutex gl_lock;
};

boost::thread_specific_ptr<ThreadVar> threadvalue;
ThreadVar *get_thread_var();
void init_threadvalue();
static Logger logger;

class LockEntity {
public:
    enum {
        TL_NONE = 0,
        TL_READ = 1,
        TL_READ_HIGH_PRIORITY = 2,
        TL_WRITE_SHARED_READ = 3,
        TL_WRITE_SHARED_WRITE = 4,
        TL_WRITE = 5, /* exclusive write */
        TL_LAST = 6,
    };

public:
    LockEntity(int id, int lock_type)
        : id_(id), lock_type_(lock_type)
    {}

    int id() const {
        return id_;
    }

    int locktype() const {
        return lock_type_;
    }

    bool is_reader() const {
        return lock_type_ == TL_READ
            || lock_type_ == TL_READ_HIGH_PRIORITY;
    }

    std::string toString() const {
        std::stringstream ss;
        ss << id_ << "/";
        ss << ( lock_type_ == TL_NONE ? "NONE"
                : lock_type_ == TL_READ ? "READ"
                : lock_type_ == TL_READ_HIGH_PRIORITY ? "READ_HIGH_PRIORITY"
                : lock_type_ == TL_WRITE_SHARED_READ ? "WRITE_SHARED_READ"
                : lock_type_ == TL_WRITE_SHARED_WRITE ? "WRITE_SHARED_WRITE"
                : "WRITE");
        return ss.str();
    }
    
private:
    int id_;
    int lock_type_;
};

class ThreadWaitQueue {
public:
    ThreadWaitQueue()
        : last_thread(NULL),
          count(0)
    {}

    void link(ThreadVar *thr);
    void unlink(ThreadVar *thr);
    ThreadVar *top() const { return last_thread; }
    void clear();

    int get_count() const { return count; }
    void reset() { last_thread = NULL; }
    bool empty() const { 
        return count == 0;
    }

private:
    ThreadVar *last_thread;
    int count;
};

class ReadThreadWaitQueue: public ThreadWaitQueue {
public:
    ReadThreadWaitQueue()
        : ThreadWaitQueue(),
          high_priority_count_(0)
    {}

    void link(ThreadVar *thr, const LockEntity &ent);
    void unlink(ThreadVar *thr, const LockEntity &ent);

    int high_priority_count() const {
        return high_priority_count_;
    }

    void check_consistency() const {
        ThreadVar *b, *e;
        e = top();

        int cnt = 0, high_prio  = 0;
        if (e) {
            cnt++;
            if (b->lock_type == 1<<LockEntity::TL_READ_HIGH_PRIORITY)
                high_prio++;
            b = e->prev;
            while (b != e) {
                assert(b);
                if (b->lock_type == 1<<LockEntity::TL_READ_HIGH_PRIORITY)
                    high_prio++;
                cnt++;
                b = b->prev;
            }
            assert(cnt == get_count());
        }
        assert(high_prio == high_priority_count());
    }

private:
    int high_priority_count_;
};

class WriteThreadWaitQueue: public ThreadWaitQueue {
public:
    WriteThreadWaitQueue()
        : ThreadWaitQueue(),
          shared_read_count_(0),
          shared_write_count_(0)
    {}

    void link(ThreadVar *thr, const LockEntity &ent);
    void unlink(ThreadVar *thr, const LockEntity &ent);

    int shared_read_count() const {
        return shared_read_count_;
    }

    int shared_write_count() const {
        return shared_write_count_;
    }

    void check_consistency() const {
        ThreadVar *b, *e;
        e = top();

        int cnt = 0;
        if (e) {
            cnt++;
            b = e->prev;
            while (b != e) {
                assert(b);
                cnt++;
                b = b->prev;
            }
            assert(cnt == get_count());
        }
    }

private:
    int shared_read_count_;
    int shared_write_count_;
};

ThreadVar *
get_thread_var() { 
    return static_cast<ThreadVar*>(threadvalue.get()); 
}

void
init_threadvalue() {
    threadvalue.reset(new ThreadVar());
}

void
ThreadWaitQueue::link(ThreadVar *thr) {
    assert(count >= 0);
    if (last_thread) {
        thr->next = last_thread;
        thr->prev = last_thread->prev;
        thr->prev->next = thr;
        last_thread->prev = thr;
    } else {
        assert(count == 0);
        thr->next = thr;
        thr->prev = thr;
    }
    last_thread = thr;
    count++;
}

void
ThreadWaitQueue::unlink(ThreadVar *thr) {
    assert(count > 0);
    if (count == 1) {
        last_thread = NULL;
    } else if (last_thread == thr) {
        last_thread = last_thread->next;         
    }
    if (thr->prev)
        thr->prev->next = thr->next;
    if (thr->next)
        thr->next->prev = thr->prev;
    thr->prev = NULL;
    thr->next = NULL;
    count--;
}

void
ThreadWaitQueue::clear() {
    ThreadVar *last = last_thread;
    for (; last; last = last->next) {
        last->suspend.notify_one();
    }
}

void
ReadThreadWaitQueue::link(ThreadVar *thr, const LockEntity &ent) {
    if (ent.locktype() == LockEntity::TL_READ_HIGH_PRIORITY) {
        ++high_priority_count_;
    }
    this->ThreadWaitQueue::link(thr);
}

void
ReadThreadWaitQueue::unlink(ThreadVar *thr, const LockEntity &ent) {
    if (ent.locktype() == LockEntity::TL_READ_HIGH_PRIORITY) {
        --high_priority_count_;
    }
    this->ThreadWaitQueue::unlink(thr);
}

void
WriteThreadWaitQueue::link(ThreadVar *thr, const LockEntity &ent) {
    if (ent.locktype() == LockEntity::TL_WRITE_SHARED_READ) {
        ++shared_read_count_;
    }
    if (ent.locktype() == LockEntity::TL_WRITE_SHARED_WRITE) {
        ++shared_write_count_;
    }
    this->ThreadWaitQueue::link(thr);
}

void
WriteThreadWaitQueue::unlink(ThreadVar *thr, const LockEntity &ent) {
    if (ent.locktype() == LockEntity::TL_WRITE_SHARED_READ) {
        --shared_read_count_;
    }
    if (ent.locktype() == LockEntity::TL_WRITE_SHARED_WRITE) {
        --shared_write_count_;
    }
    this->ThreadWaitQueue::unlink(thr);
}

class LockManager {
public:
    LockManager()
        : can_be_used(true)
    {}

    ~LockManager();

    void start();
    void stop();
    bool lock(const std::vector<LockEntity> &lst);
    bool unlock(const std::vector<LockEntity> &lst);

private:
    bool lock_entity(const LockEntity &ent);
    bool unlock_entity(const LockEntity &ent);

    void wait_for_readers(const LockEntity &ent, bool all, ThreadWaitQueue &wq);
    void wait_for_writers(const LockEntity &ent, bool all, ThreadWaitQueue &wq);
    void wait_for_all_readers();
    void wait_for_all_writers();

    void signal_readers(int id, bool all, int mask);
    void signal_writers(int id, bool all, int mask);

    bool check_lock_consistency(const std::vector<LockEntity> &lst);
    const std::string toString(int id);
    void check_queue_consistency(int id);
    void check_all_queues_consistency();

private:
    typedef std::map<int, ReadThreadWaitQueue> ReadWaitQueuesMap;
    typedef std::map<int, WriteThreadWaitQueue> WriteWaitQueuesMap;

    bool can_be_used;

    ReadWaitQueuesMap readers_running;
    WriteWaitQueuesMap writers_running;

    ReadWaitQueuesMap readers_queued;
    WriteWaitQueuesMap writers_queued;

    boost::mutex gl_lock;
};

void
LockManager::check_all_queues_consistency() {
    ReadWaitQueuesMap::const_iterator it = readers_running.begin();
    for (; it != readers_running.end(); ++it) {
        check_queue_consistency(it->first);
        /* logger.logit("CHECK: " + toString(it->first)); */
    }
}

void 
LockManager::check_queue_consistency(int id) 
{
    ReadThreadWaitQueue &rrunning = readers_running[id];
    WriteThreadWaitQueue &wrunning = writers_running[id];
    ReadThreadWaitQueue &rqueued = readers_queued[id];
    WriteThreadWaitQueue &wqueued = writers_queued[id];
    rrunning.check_consistency();
    wrunning.check_consistency();
    rqueued.check_consistency();
    wqueued.check_consistency();
}

bool
LockManager::lock(const std::vector<LockEntity> &lst) {
    boost::mutex::scoped_lock scoped_lock(gl_lock);

    if (!can_be_used) {
        return false;
    }

    if (!check_lock_consistency(lst)) {
        return false;
    }

    int i, j;
    bool success = true;
    for (i = 0; i < lst.size(); ++i) {
        if (!lock_entity(lst[i])) {
            success = false;
            break;
        }
    }
    if (!success) {
        for (j = i - 1; j >= 0; --j) {
            if (!unlock_entity(lst[j]))
                return false;
        }
    }
    return i == lst.size();
}

bool
LockManager::unlock(const std::vector<LockEntity> &lst) {
    boost::mutex::scoped_lock scoped_lock(gl_lock);

    if (!can_be_used) {
        return false;
    }

    int i;
    for (i = 0; i < lst.size(); ++i) {
        if (!unlock_entity(lst[i]))
            break;
    }
    return i == lst.size();
}

void
LockManager::wait_for_readers(const LockEntity &ent, bool all, ThreadWaitQueue &wq) 
{
    ThreadVar *thr = get_thread_var();

    assert(thr);
    const ThreadWaitQueue &rrunning = readers_running[ent.id()];
    const ThreadWaitQueue &rqueued = readers_queued[ent.id()];
    check_all_queues_consistency();check_queue_consistency(ent.id());
    wq.link(thr);
    check_all_queues_consistency();check_queue_consistency(ent.id());
    if (all) {
        while (!rrunning.empty() || !rqueued.empty()) {
            check_all_queues_consistency();check_queue_consistency(ent.id());
            thr->wait_on_lock(gl_lock, ent.locktype());
        }
    } else {
        while (!rrunning.empty()) {
            check_all_queues_consistency();check_queue_consistency(ent.id());
            thr->wait_on_lock(gl_lock, ent.locktype());
        }
    }
    check_all_queues_consistency();check_queue_consistency(ent.id());
    wq.unlink(thr);
    check_all_queues_consistency();check_queue_consistency(ent.id());
}

void
LockManager::wait_for_writers(const LockEntity &ent, bool all, ThreadWaitQueue &wq) 
{
    ThreadVar *thr = get_thread_var();

    assert(thr);
    const ThreadWaitQueue &wrunning = writers_running[ent.id()];
    const ThreadWaitQueue &wqueued = writers_queued[ent.id()];
    check_all_queues_consistency();check_queue_consistency(ent.id());
    wq.link(thr);
    check_all_queues_consistency();check_queue_consistency(ent.id());
    if (all) {
        while (!wrunning.empty() || !wqueued.empty()) {
            check_all_queues_consistency();check_queue_consistency(ent.id());
            thr->wait_on_lock(gl_lock, ent.locktype());
        }
    } else {
        while (!wrunning.empty()) {
            check_all_queues_consistency();check_queue_consistency(ent.id());
            thr->wait_on_lock(gl_lock, ent.locktype());
        }
    }
    check_all_queues_consistency();check_queue_consistency(ent.id());
    wq.unlink(thr);
    check_all_queues_consistency();check_queue_consistency(ent.id());
}


bool 
LockManager::lock_entity(const LockEntity &ent) {
    ThreadVar *thr = get_thread_var();

    assert(thr);
    if (!can_be_used) {
        return false;
    }

    ReadThreadWaitQueue &rrunning = readers_running[ent.id()];
    WriteThreadWaitQueue &wrunning = writers_running[ent.id()];
    ReadThreadWaitQueue &rqueued = readers_queued[ent.id()];
    WriteThreadWaitQueue &wqueued = writers_queued[ent.id()];

    logger.logit("LOCK: " + ent.toString());

    check_all_queues_consistency();check_queue_consistency(ent.id());

    bool res = true;
    switch (ent.locktype()) {
    case LockEntity::TL_READ:
        if (rrunning.empty()) {
            wait_for_writers(ent, true, rqueued);
        }
        rrunning.link(thr, ent);
    break;

    case LockEntity::TL_READ_HIGH_PRIORITY:
        if (rrunning.empty()) {
            wait_for_writers(ent, false, rqueued);
        }
        rrunning.link(thr, ent);
    break;

    case LockEntity::TL_WRITE_SHARED_READ:
        wait_for_writers(ent, false, wqueued);
        wrunning.link(thr, ent);
    break;

    case LockEntity::TL_WRITE_SHARED_WRITE:
        wait_for_readers(ent, false, wqueued);
        wrunning.link(thr, ent);
    break;

    case LockEntity::TL_WRITE:
        while (!wrunning.empty() || !rrunning.empty()) {
            wait_for_readers(ent, false, wqueued);
            wait_for_writers(ent, false, wqueued);
        }
        wrunning.link(thr, ent);
    break;

    default:
        assert(0);
    };
    logger.logit("LOCK: " + toString(ent.id()));
    check_all_queues_consistency();check_queue_consistency(ent.id());
    return res;
}

void
LockManager::signal_readers(int id, bool all, int mask = 0) {
    const ThreadWaitQueue &rqueued = readers_queued[id];

    if (rqueued.empty())
        return;

    check_all_queues_consistency();check_queue_consistency(id);

    ThreadVar *beg, *end;
    end = rqueued.top();
    beg = end->prev;

    do {
        if (!mask || mask & beg->lock_type) {
            beg->suspend.notify_one();
        }

        if (!all)
            break;

        beg = beg->prev;
    } while (beg != end);

    check_all_queues_consistency();check_queue_consistency(id);
}

void
LockManager::signal_writers(int id, bool all, int mask = 0) {
    const ThreadWaitQueue &wqueued = writers_queued[id];

    if (wqueued.empty())
        return;

    check_all_queues_consistency();check_queue_consistency(id);

    ThreadVar *beg, *end;
    end = wqueued.top();
    beg = end->prev;

    do {
        if (!mask || mask & beg->lock_type) {
            beg->suspend.notify_one();
        }

        if (!all)
            break;
        
        beg = beg->prev;
    } while (beg != end);

    check_all_queues_consistency();check_queue_consistency(id);
}

bool 
LockManager::unlock_entity(const LockEntity &ent) {
    ThreadVar *thr = get_thread_var();

    assert(thr);
    if (!can_be_used) {
        return false;
    }

    ReadThreadWaitQueue &rrunning = readers_running[ent.id()];
    WriteThreadWaitQueue &wrunning = writers_running[ent.id()];
    ReadThreadWaitQueue &rqueued = readers_queued[ent.id()];
    WriteThreadWaitQueue &wqueued = writers_queued[ent.id()];

    logger.logit("UNLOCK: " + ent.toString());

    check_all_queues_consistency();check_queue_consistency(ent.id());

    bool res = true;
    switch (ent.locktype()) {
    case LockEntity::TL_READ:
    case LockEntity::TL_READ_HIGH_PRIORITY: {
        assert(!rrunning.empty());
        if (rrunning.get_count() == 1) {
            signal_writers(ent.id(), false);
        } 
        rrunning.unlink(thr, ent);
        break;
    }

    case LockEntity::TL_WRITE_SHARED_READ: {
        assert(!wrunning.empty());
        if (wrunning.get_count() == 1) {
            signal_writers(ent.id(), false);
        }
        wrunning.unlink(thr, ent);
        break;
    }

    case LockEntity::TL_WRITE:
    case LockEntity::TL_WRITE_SHARED_WRITE: {
        assert(!wrunning.empty());
        wrunning.unlink(thr, ent);
        if (wrunning.get_count() == 0) {
            if (rqueued.high_priority_count() > 0) {
                signal_readers(ent.id(), true, 
                    1 << LockEntity::TL_READ_HIGH_PRIORITY);
            } else if (!wqueued.empty()) {
                signal_writers(ent.id(), false);
            } else {
                signal_readers(ent.id(), true);
            }
        } 
        break;

    default:
        assert(0);
    }};
    logger.logit("UNLOCK: " + toString(ent.id()));
    check_all_queues_consistency();check_queue_consistency(ent.id());
    return res;
}

void
LockManager::wait_for_all_readers() {
    ReadWaitQueuesMap::const_iterator it = readers_running.begin();
    for (; it != readers_running.end(); ++it) {
        int id = it->first;
        ThreadWaitQueue &rqueued = readers_queued[id];
        ThreadWaitQueue &wqueued = writers_queued[id];
        if (!rqueued.empty()) {
            wait_for_readers(LockEntity(id, LockEntity::TL_NONE), true, wqueued);
        }
        assert(rqueued.empty());
        assert(it->second.empty());
    }
}

void
LockManager::wait_for_all_writers() {
    WriteWaitQueuesMap::const_iterator it = writers_running.begin();
    for (; it != writers_running.end(); ++it) {
        int id = it->first;
        ThreadWaitQueue &rqueued = readers_queued[id];
        ThreadWaitQueue &wqueued = readers_queued[id];
        if (!wqueued.empty()) {
            wait_for_writers(LockEntity(id, LockEntity::TL_NONE), true, rqueued);
        }
        assert(wqueued.empty());
        assert(it->second.empty());
    }
}

bool
LockManager::check_lock_consistency(const std::vector<LockEntity> &lst) {
    /* 

    0. TL_NONE
    1. TL_READ
    2. TL_READ_HIGH_PRIORITY
    3. TL_WRITE_SHARED_READ
    4. TL_WRITE_SHARED_WRITE
    5. TL_WRITE

      0 1 2 3 4 5
    0 1 1 1 1 1 1
    1 1 1 1 1 0 0
    2 1 1 1 1 0 0
    3 1 1 1 1 1 0
    4 1 0 0 1 1 0
    5 1 0 0 0 0 0

    */

    static const int consistent[LockEntity::TL_LAST][LockEntity::TL_LAST] = {
        { 1, 1, 1, 1, 1, 1 },
        { 1, 1, 1, 1, 0, 0 },
        { 1, 1, 1, 1, 0, 0 },
        { 1, 1, 1, 1, 1, 0 },
        { 1, 0, 0, 1, 1, 0 },
        { 1, 0, 0, 0, 0, 0 },
    };

    for (int i = 1; i < lst.size(); ++i) {
        for (int j = i - 1; j >= 0; --j) {
            if (!consistent[lst[i].locktype()][lst[j].locktype()]) {
                return false;
            }
        }
    }
    return true;
}

const std::string 
LockManager::toString(int id) {
    std::stringstream ss;
    ReadThreadWaitQueue &rrunning = readers_running[id];
    WriteThreadWaitQueue &wrunning = writers_running[id];
    ReadThreadWaitQueue &rqueued = readers_queued[id];
    WriteThreadWaitQueue &wqueued = writers_queued[id];

    ss << id << ": ";
    ss << "(" << rrunning.get_count() 
        << "," << rrunning.high_priority_count() << ")";
    ss << "(" << rqueued.get_count() 
        << "," << rqueued.high_priority_count() << ")";
    ss << "(" << wrunning.get_count() 
        << "," << wrunning.shared_read_count() 
        << "," << wrunning.shared_write_count() << ")";
    ss << "(" << wqueued.get_count() 
        << "," << wqueued.shared_read_count() 
        << "," << wqueued.shared_write_count() << ")";
    ss << "\n";

    return ss.str();
}

void
LockManager::start() {
    can_be_used = true;
}

void
LockManager::stop() {
    boost::mutex::scoped_lock scoped_lock(gl_lock);

    can_be_used = false;
    
    wait_for_all_writers();
    wait_for_all_readers();
}

LockManager::~LockManager() {
    stop();
}

/* tests from mysql  mysys/thr_lock.c */
struct st_test {
  uint lock_nr;
  int lock_type;
};

struct st_test test_0[] = {{0,LockEntity::TL_READ}};	/* One lock */
struct st_test test_1[] = {{0,LockEntity::TL_READ},{0,LockEntity::TL_WRITE}}; /* Read and write lock of lock 0 */
struct st_test test_2[] = {{1,LockEntity::TL_WRITE},{0,LockEntity::TL_READ},{2,LockEntity::TL_READ}};
struct st_test test_3[] = {{2,LockEntity::TL_WRITE},{1,LockEntity::TL_READ},{0,LockEntity::TL_READ}}; /* Deadlock with test_2 ? */
struct st_test test_4[] = {{0,LockEntity::TL_WRITE},{0,LockEntity::TL_READ},{0,LockEntity::TL_WRITE},{0,LockEntity::TL_READ}};
struct st_test test_5[] = {{0,LockEntity::TL_READ},{1,LockEntity::TL_READ},{2,LockEntity::TL_READ},{3,LockEntity::TL_READ}}; /* Many reads */
struct st_test test_6[] = {{0,LockEntity::TL_WRITE},{1,LockEntity::TL_WRITE},{2,LockEntity::TL_WRITE},{3,LockEntity::TL_WRITE}}; /* Many writes */
struct st_test test_7[] = {{3,LockEntity::TL_READ}};
struct st_test test_8[] = {{1,LockEntity::TL_READ},{2,LockEntity::TL_READ},{3,LockEntity::TL_READ}};	/* Should be quick */
struct st_test test_9[] = {{4,LockEntity::TL_READ_HIGH_PRIORITY}};
struct st_test test_10[] ={{4,LockEntity::TL_WRITE}};
struct st_test test_11[] = {{0,LockEntity::TL_WRITE_SHARED_WRITE},{1,LockEntity::TL_WRITE_SHARED_WRITE},{2,LockEntity::TL_WRITE_SHARED_WRITE},{3,LockEntity::TL_WRITE_SHARED_WRITE}}; /* Many writes */
struct st_test test_12[] = {{0,LockEntity::TL_WRITE_SHARED_WRITE},{1,LockEntity::TL_WRITE_SHARED_WRITE},{2,LockEntity::TL_WRITE_SHARED_WRITE},{3,LockEntity::TL_WRITE_SHARED_WRITE}}; /* Many writes */
struct st_test test_13[] = {{0,LockEntity::TL_WRITE_SHARED_WRITE},{1,LockEntity::TL_WRITE_SHARED_WRITE},{2,LockEntity::TL_WRITE_SHARED_WRITE},{3,LockEntity::TL_WRITE_SHARED_WRITE}};
struct st_test test_14[] = {{0,LockEntity::TL_WRITE_SHARED_WRITE},{1,LockEntity::TL_READ}};
struct st_test test_15[] = {{0,LockEntity::TL_WRITE_SHARED_WRITE},{1,LockEntity::TL_READ}};
struct st_test test_16[] = {{0,LockEntity::TL_WRITE_SHARED_WRITE},{1,LockEntity::TL_WRITE_SHARED_WRITE}};

struct st_test *tests[] = {test_0,test_1,test_2,test_3,test_4,test_5,test_6,
			   test_7,test_8,test_9,test_10,test_11,test_12,
			   test_13,test_14,test_15,test_16};
int lock_counts[]= {sizeof(test_0)/sizeof(struct st_test),
		    sizeof(test_1)/sizeof(struct st_test),
		    sizeof(test_2)/sizeof(struct st_test),
		    sizeof(test_3)/sizeof(struct st_test),
		    sizeof(test_4)/sizeof(struct st_test),
		    sizeof(test_5)/sizeof(struct st_test),
		    sizeof(test_6)/sizeof(struct st_test),
		    sizeof(test_7)/sizeof(struct st_test),
		    sizeof(test_8)/sizeof(struct st_test),
		    sizeof(test_9)/sizeof(struct st_test),
		    sizeof(test_10)/sizeof(struct st_test),
		    sizeof(test_11)/sizeof(struct st_test),
		    sizeof(test_12)/sizeof(struct st_test),
		    sizeof(test_13)/sizeof(struct st_test),
		    sizeof(test_14)/sizeof(struct st_test),
		    sizeof(test_15)/sizeof(struct st_test),
		    sizeof(test_16)/sizeof(struct st_test)
};

class TestThread {
public:
    class RandomGenerator {
    public:
        RandomGenerator(int initial_seed)
            : seed(initial_seed)
        {} 

        int operator()() {
            return rand_r(&seed);
        }

    private:
        unsigned int seed;
    };

public:
    TestThread(LockManager *lm)
        : lm_(lm)
    {}

    void operator()() {
        init_threadvalue();
        RandomGenerator gen(time(NULL));
        int lock_counts_num = sizeof(lock_counts) / sizeof(lock_counts[0]);

        for (int i = 0; i < 100; ++i) {
            int idx = gen() % lock_counts_num;
            struct st_test *test = tests[idx];
            std::vector<LockEntity> vec;
            for (int j = 0; j < lock_counts[idx]; ++j) {
                vec.push_back(LockEntity(test->lock_nr, test->lock_type));
                test++;
            }
            if (lm_->lock(vec)) {
                for (int k = 0; k < gen() * 1000; ++k) {
                    boost::thread::yield();
                }
                lm_->unlock(vec);
            }
        }
    }

private:
    LockManager *lm_;
};

int main() {
    const int NUMBER_OF_THREADS = 5;
    init_threadvalue();

    LockManager lm;
    boost::thread_group thrgroup;

    std::vector<TestThread> threads;
    for (int i = 0; i < NUMBER_OF_THREADS; ++i) {
        threads.push_back(TestThread(&lm));
        thrgroup.create_thread(threads.back());
    }
    thrgroup.join_all();

    return 0;
}
