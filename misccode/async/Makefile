
CC = gcc
CFLAGS = -g -std=c99
LDFLAGS = -levent

all: server client

server: server.c logger.c packet.h
	${CC} -o $@ $^ ${CFLAGS} ${LDFLAGS}

client: client.c logger.c packet.h
	${CC} -o $@ $^ ${CFLAGS} ${LDFLAGS}

clean:
	@rm server client
