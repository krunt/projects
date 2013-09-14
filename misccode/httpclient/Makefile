
CC=gcc
CFLAGS=-g

OFILES=alloc_operations.o connection.o error.o file_operations.o http.o utils.o

%.o: %.c
	$(CC) $(CFLAGS) -c $^

httplib: $(OFILES)
	$(CC) $(CFLAGS) -shared -o lib$@.so $(OFILES) -Wl,-rpath,$(PWD)

client: httplib client.c
	$(CC) $(CFLAGS) -o $@ client.c -L$(PWD) -lhttplib
	
clean:
	rm -f *.o *.so client
