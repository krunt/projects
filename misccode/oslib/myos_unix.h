#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#define MYOS_EAGAIN EAGAIN
#define MYOS_EINTR EINTR

typedef int fdsocket_t;
typedef int fdhandle_t;
