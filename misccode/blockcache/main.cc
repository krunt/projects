#include "blockcache.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>

const char *filepath = "dragon_with_smile";
const int blocksize = 1024;
const int usemem = 16*1024*1024;
const int READER_THREADS = 22;
const int WRITER_THREADS = 1;

class TestThread {
private:
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
    enum { READER, WRITER };

    TestThread(BlockCache *bcache, int bfd, int brole)
        : cache(bcache),
          fd(bfd),
          role(brole)
    {}

    const std::string id() const {
        std::stringstream ss;
        ss << boost::this_thread::get_id();
        switch (ss.str().length()) {
        case 2:
            return ss.str() + "0";
        case 1:
            return ss.str() + "00";
        case 0:
            return "000";
        default:
            return ss.str();
        };
    }

    void operator()() {
        int i;
        char buf[blocksize+1];
        init_threadvalue();

        const std::string &xid = id();
        RandomGenerator rgen(xid[0] * 26 * 26 + xid[1] * 26 + xid[2]);
    
        /*
        if (role == READER) {
            sleep(1);
        }
        */
    
        if (role == WRITER) {
            memset(buf, 'a', sizeof(buf));
            for (i = 0; i < 10; ++i) {
                /*
                if (cache->write(fd, buf, sizeof(buf), i * cache->blocksize())) {
                    std::cerr << "key_cache write error" << std::endl;
                    break;
                }
                */
            }
        } else if (role == READER) {
            for (i = 0; i < 5000; ++i) {
                if (cache->read(fd, buf, blocksize, 
                        (rgen() % 10000) * cache->blocksize())) 
                {
                    std::cerr << "key_cache read error" << std::endl;
                    break;
                }
                {
                    boost::mutex::scoped_lock(gllock);
                    std::cerr << id() << ": " << buf << std::endl;
                }
            }
        }
    }

private:
    BlockCache *cache;
    int fd;
    int role;

    static boost::mutex gllock;
};

int main() {
    int fd;
    if ((fd = open(filepath, O_RDONLY,
            S_IRUSR | S_IWUSR | S_IWGRP | S_IWGRP | S_IROTH))
            == -1)
    {
        std::cerr << "open() error" << std::endl;
        return 1;
    }

    std::vector<TestThread> threads;
    BlockCache cache(blocksize, usemem);
    boost::thread_group thrgroup;
    for (int i = 0; i < WRITER_THREADS; ++i) {
        threads.push_back(TestThread(&cache, fd, TestThread::WRITER));
        thrgroup.create_thread(threads.back());
    }
    for (int i = 0; i < READER_THREADS; ++i) {
        threads.push_back(TestThread(&cache, fd, TestThread::READER));
        thrgroup.create_thread(threads.back());
    }
    thrgroup.join_all();
    close(fd);

    return 0;
}
