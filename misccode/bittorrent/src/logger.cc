#include <include/logger.h>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>

#include <stdarg.h>

namespace btorrent {

void logger_t::loghelper(logger_level_t level, 
        const std::string &format, va_list ap) const 
{
    if (m_level > level) {
        return;
    }

    boost::mutex::scoped_lock lk(m_lock);
    boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
    fprintf(m_fd, "[%s]:%s:[%s]: ",
            boost::posix_time::to_iso_string(now).c_str(),
            level == kinfo ? "INFO" : level == kwarn ? "WARN" : 
            level == kerror ? "ERROR" : "DEBUG", m_namespace.c_str());
    vfprintf(m_fd, format.c_str(), ap);
    fprintf(m_fd, "\n");
}

void logger_t::debug(const std::string &format, ...) const { 
    va_list ap;
    va_start(ap, format);
    loghelper(kdebug, format, ap); 
    va_end(ap);
}

void logger_t::info(const std::string &format, ...) const { 
    va_list ap;
    va_start(ap, format);
    loghelper(kinfo, format, ap); 
    va_end(ap);
}

void logger_t::warn(const std::string &format, ...) const { 
    va_list ap;
    va_start(ap, format);
    loghelper(kwarn, format, ap); 
    va_end(ap);
}

void logger_t::error(const std::string &format, ...) const { 
    va_list ap;
    va_start(ap, format);
    loghelper(kerror, format, ap); 
    va_end(ap);
}

void logger_t::set_namespace(const std::string &namespace_name) {
    m_namespace = namespace_name;
}

void logger_t::set_level(logger_t::logger_level_t level) {
    m_level = level; 
}

logger_t global_logger;
extern logger_t *glog(const char *method_name) { 
    global_logger.set_namespace(method_name ? method_name : "");
    return &global_logger; 
}

}
