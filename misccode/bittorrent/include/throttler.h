#ifndef THROTTLER_DEF_
#define THROTTLER_DEF_

#include <include/common.h>
#include <sys/time.h>

namespace btorrent {

/* bytes per-interval throttler */
class throttler_t {
public:
    throttler_t(int seconds, size_type bytes) 
        : m_seconds(seconds),
          m_bytes_limit(bytes)
    { reset(); }

    void account(size_type bytes);

    size_type available();
    bool expired() const;

private:
    void reset();

    int m_seconds;
    size_type m_bytes_got;
    size_type m_bytes_limit;
    struct timeval m_last_timestamp;
};

}

#endif /* THROTTLER_DEF_ */
