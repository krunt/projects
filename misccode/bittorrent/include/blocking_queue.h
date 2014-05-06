#ifndef BLOCKING_QUEUE_DEF_
#define BLOCKING_QUEUE_DEF_

#include <include/common.h>
#include <deque>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>

namespace btorrent {

template <typename T, int sz>
class blocking_queue_t {
public:
    blocking_queue_t() {}

    bool empty() {
        boost::mutex::scoped_lock lk(m_lock);
        return m_queue.empty();
    }

    void push(const T &v) {
        boost::mutex::scoped_lock lk(m_lock);

        while (m_queue.size() >= sz)
            m_queue_full.wait(lk);

        m_queue.push_back(v);
        m_queue_not_empty.notify_one();
    }

    T pop() {
        boost::mutex::scoped_lock lk(m_lock);
        
        while (m_queue.empty())
            m_queue_not_empty.wait(lk);

        T result = m_queue.front();
        m_queue.pop_front();

        m_queue_full.notify_one();

        return result;
    }

private:
    boost::mutex m_lock; 
    boost::condition m_queue_full, m_queue_not_empty;
    std::deque<T> m_queue;
};

}

#endif /* BLOCKING_QUEUE_DEF_ */
