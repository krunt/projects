
#include "myos.h"

void win32_init(void) {}

myos_t myos_win32 = {
    .init = &win32_init,
};

