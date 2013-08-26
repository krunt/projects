#ifndef CONFIG_SE__
#define CONFIG_SE__

#define MAX_PIDS 128
#define PERIOD_INTERVAL 1
#define INVALID  -1

//#define COMMON_PREFIX "/home/alexey/cpp/experiments/shared_engine/tmp"
#define COMMON_PREFIX "/tmp"

#define SOCKET_PREFIX COMMON_PREFIX "/socket-%d"
#define SEM_IDENT_PATH COMMON_PREFIX "/sem_keyid"
#define MEM_IDENT_PATH COMMON_PREFIX "/mem_keyid"

#endif
