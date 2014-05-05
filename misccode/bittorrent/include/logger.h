#ifndef LOGGER_DEF_
#define LOGGER_DEF_

#include "common.h"
#include <boost/noncopyable.hpp>

namespace btorrent {

class logger_t: public boost::noncopyable {
public:
    enum logger_level_t { kdebug = 0, kinfo = 1, kwarn = 2, kerror = 3, 
        kdisabled = kerror + 1  };

    logger_t(FILE *fd = stderr)
        : m_fd(fd), m_level(kinfo)
    {}

    void debug(const std::string &format, ...) const;
    void info(const std::string &format, ...) const;
    void warn(const std::string &format, ...) const;
    void error(const std::string &format, ...) const;
    void set_namespace(const std::string &namespace_name);
    void set_level(logger_level_t level);

private:

    void loghelper(logger_level_t level, 
            const std::string &format, va_list va) const;

    FILE *m_fd;
    std::string m_namespace;
    logger_level_t m_level;
};

logger_t *glog(const char *method_name = NULL);

#define DEFINE_METHOD(return_type, method_name, ...) \
    return_type method_name(__VA_ARGS__) { \
        const char *method_name_str = #method_name;
#define DEFINE_CONST_METHOD(return_type, method_name, ...) \
    return_type method_name(__VA_ARGS__) const { \
        const char *method_name_str = #method_name;
#define END_METHOD }

#define GLOG glog(method_name_str)

}

#endif /* LOGGER_DEF_ */
