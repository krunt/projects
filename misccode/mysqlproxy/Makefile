
CC=g++
CCFLAGS=-g -O0

OFILES=mysqlproxy.o mysqlvio.o mysqlcompress.o

%.o: %.cc
	$(CC) $(CCFLAGS) -c $^

mysqlproxy: $(OFILES) mysqlcommon.h mysqlvio.h
	$(CC) $(CCFLAGS) -o mysqlproxy $(OFILES) -levent -lssl -lz

clean:
	rm -f *.log *.o mysqlproxy

