CC := gcc
RM := rm
CFLAGS = -g -std=c99 -Wall
LDFLAGS = -lmysqlclient

all: mysql_lookup

mysql_lookup: mysql_lookup.c
	${CC} -o $@ $< ${CFLAGS} ${LDFLAGS}

.PHONY = clean

clean:
	${RM} -f *.o mysql_insert
