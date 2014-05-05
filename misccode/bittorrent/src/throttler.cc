
#include "include/throttler.h"

namespace btorrent {

void throttler_t::reset() {
    m_bytes_got = 0;
    gettimeofday(&m_last_timestamp, NULL);
}

DEFINE_CONST_METHOD(bool, throttler_t::expired)
    size_type udiff;
    struct timeval now;
    gettimeofday(&now, NULL);

    udiff = (now.tv_sec - m_last_timestamp.tv_sec) * 1000000
        + now.tv_usec - m_last_timestamp.tv_usec;

    return udiff / 1000 > m_seconds * 1000;
END_METHOD


void throttler_t::account(size_type bytes) {
    if (expired()) {
        reset();
    }

    if (m_bytes_got >= m_bytes_limit) {
        return;
    }

    m_bytes_got += bytes;
}

size_type throttler_t::available() {
    if (expired()) {
        reset();
    }

    size_type bytes_available = m_bytes_limit - m_bytes_got;
    return bytes_available > 0 ? bytes_available : 0;
}

}
