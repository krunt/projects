#ifndef LOGGER_DEF_
#define LOGGER_DEF_

#include "common.h"
#include <boost/noncopyable.hpp>

namespace btorrent {

class logger_t: public boost::noncopyable {
public:
    logger_t(FILE *fd = stderr)
        : m_fd(fd)
    {}

    void debug(const std::string &format, ...) const;
    void info(const std::string &format, ...) const;
    void warn(const std::string &format, ...) const;
    void error(const std::string &format, ...) const;

private:

    enum logger_level_t { kinfo = 0, kwarn = 1, kerror = 2, kdebug = 3, };
    void loghelper(logger_level_t level, 
            const std::string &format, va_list va) const;

    FILE *m_fd;
};

extern logger_t *glog();

}

#endif /* LOGGER_DEF_ */
