#include "logger.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

static int level = 6;
static FILE * fd; 
static const char * fn = 0;
static int mode = 0;
static const char * timeformat = "%d.%m.%Y-%H:%M:%S";

static void logger_new_file();

void logger_init(const char *s) {
    fn = s;
    logger_new_file();
}

static void logger_new_file() {
    if (fd) {
        fclose(fd); fd = 0;
    }
    if (fn) {
        fd = fopen(fn, "w");
        if (!fd) {
            return;
        }
        setbuf(fd, 0);
    }
}

void my_log(int type, const char * format, ...) {
    char time_buf[1024];
    va_list ap;
    struct tm *tm = 0;
    time_t t;

    if (type > level) {
        return;
    }

    time(&t);
    tm = localtime(&t);

    if (timeformat) {
        strftime(time_buf, sizeof(time_buf), timeformat, tm);
        time_buf[sizeof(time_buf) - 1] = 0;
    }

    va_start(ap, format);

    if (timeformat) {
        fprintf(fd, "%s: ", time_buf);
    }

    switch (type) {
      case LOG_EMERG:
        fprintf(fd, "EMERGE:\t");   
        break;
      case LOG_CRIT:
        fprintf(fd, "CRIT:\t");
        break;
      case LOG_ERR:
        fprintf(fd, "ERR:\t");
        break;
      case LOG_WARNING:
        fprintf(fd, "WARN:\t");
        break;
      case LOG_NOTICE:
        fprintf(fd, "NOTICE:\t");
        break;
      case LOG_INFO:
        fprintf(fd, "INFO:\t");
        break;
      case LOG_DEBUG:
      default:
        fprintf(fd, "DEBUG:\t");
        break;
    };

    vfprintf(fd, format, ap);
    fprintf(fd, "\n");

    va_end(ap);
}


