#ifndef MYCLIB_LOGGER_
#define MYCLIB_LOGGER_

#include <syslog.h>

void logger_init(const char *s);
void my_log(int type, const char * format, ...);

#define my_log_debug(...) my_log(LOG_DEBUG, __VA_ARGS__)
#define my_log_info(...) my_log(LOG_INFO, __VA_ARGS__)
#define my_log_error(...) my_log(LOG_ERR, __VA_ARGS__)
#define my_log_crit(...) my_log(LOG_CRIT, __VA_ARGS__)

#endif /* MYCLIB_LOGGER_ */
