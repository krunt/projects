#include <include/logger.h>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <stdarg.h>

namespace btorrent {

void logger_t::loghelper(logger_level_t level, 
        const std::string &format, va_list ap) const 
{
    boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
    fprintf(m_fd, "[%s]:%5s: ",
            boost::posix_time::to_iso_string(now).c_str(),
            level == kinfo ? "INFO" : level == kwarn ? "WARN" : 
            level == kerror ? "ERROR" : "DEBUG");
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

logger_t global_logger;
extern logger_t *glog() { return &global_logger; }

}
