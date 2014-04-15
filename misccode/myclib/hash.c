#include "hash.h"

unsigned int bobj_hash(char *str, int len) {
    unsigned int res = 0;
    while (len--) {
        res += *str++;
        res += (res << 10);
        res ^= (res >> 6);
    }
    res += (res << 3);
    res ^= (res >> 11);
    return res + res << 15;
}

