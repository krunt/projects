CC := gcc
RM := rm
CFLAGS = -g -std=c99 -Wall -O2
LDFLAGS = -levent -levent_pthreads -llua -lmysqlclient -lpthread

all: main main_without_threads

main: main.c
	${CC} -o $@ $< ${CFLAGS} ${LDFLAGS}

main_without_threads: main_without_threads.c
	${CC} -o $@ $< ${CFLAGS} ${LDFLAGS}

.PHONY = clean

clean:
	${RM} -f *.o main main_without_threads
