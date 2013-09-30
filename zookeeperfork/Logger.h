#ifndef _LOGGER_
#define _LOGGER_

#include <syslog.h>

void logger_init(const char *s);
void mlog(int type, const char * format, ...);

#endif 
