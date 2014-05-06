
#include "myos.h"

myos_t *myos() {
#ifdef WIN32
    return &myos_win32;
#else
    return &myos_unix;
#endif
}
