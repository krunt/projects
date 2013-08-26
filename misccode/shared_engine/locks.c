#define _XOPEN_SOURCE

#include "locks.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <strings.h>
#include <fcntl.h>
#include "config.h"

int
sem_lock() {
    key_t keyid = ftok(SEM_IDENT_PATH, 128);
    if (keyid == -1) 
        return -1;

    int id = semget(keyid, 1, IPC_CREAT | 0777);
    if (id == -1)
        return -1;

    struct sembuf sop;
    bzero((void*)&sop, sizeof(sop));
    sop.sem_num = 0;
    sop.sem_op = -1;
    
    if (semop(id, (struct sembuf *)&sop, 1) == -1)
        return -1;

    return 0;
}


int
sem_unlock() {
    key_t keyid = ftok(SEM_IDENT_PATH, 128);
    if (keyid == -1) 
        return -1;

    int id = semget(keyid, 1, IPC_CREAT | 0777);
    if (id == -1)
        return -1;

    struct sembuf sop;
    bzero((void*)&sop, sizeof(sop));
    sop.sem_num = 0;
    sop.sem_op = 1;
    
    if (semop(id, (struct sembuf *)&sop, 1) == -1)
        return -1;

    return 0;
}
