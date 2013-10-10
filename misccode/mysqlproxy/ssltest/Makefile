
CC=g++
CCFLAGS=-g -O0

OFILES=mysqlvio.o

all: sslclient sslserver

%.o: %.cc
	$(CC) $(CCFLAGS) -c $^

sslclient: $(OFILES) test-sslclient.cc
	$(CC) $(CCFLAGS) -o $@ $(OFILES) test-sslclient.cc -lpthread -lssl

sslserver: $(OFILES) test-sslserver.cc 
	$(CC) $(CCFLAGS) -o $@ $(OFILES) test-sslserver.cc -lpthread -lssl

clean:
	rm -f *.o sslclient sslserver

