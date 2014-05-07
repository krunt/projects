#include "myos.h"

#ifdef WIN32
int main() {
    return 0;
}
#else
int main() {
    int error = (*myos()->get_last_error)();
    return 0;
}
#endif
