#ifndef UTILS_DEF_
#define UTILS_DEF_

#include <myclib/all.h>

void rtmp_log_hex(const char *label, u8 *data, int len);

/* returns current time in ms */
int rtmp_gettime();

#endif /* UTILS_DEF_ */
