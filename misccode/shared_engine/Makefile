
#DIR = /home/alexey/cpp/experiments/shared_engine/tmp
DIR = /tmp/
CC = gcc
CFLAGS = -g -std=c99
LDFLAGS = -levent
FILES = config.h connection.c connection.h  \
		locks.c  locks.h  master.c  master.h  packet.c  \
		packet.h shared_state.c  shared_state.h  \
		shutdown.c  shutdown.h  state.c  state.h \
		logger.h logger.c

all: main

main: main.c ${FILES}
	${CC} -o $@ main.c ${FILES} ${CFLAGS} ${LDFLAGS}

clean:
	#@rm main
	rm ${DIR}/sem_keyid
	rm ${DIR}/mem_keyid
	rm ${DIR}/socket-*

regen:
	touch ${DIR}/sem_keyid
	touch ${DIR}/mem_keyid
