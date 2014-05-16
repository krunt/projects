
CC=gcc
OFILES=amf0.o rtmp.o tcp.o url.o rtmp_client.o utils.o flv.o
CFLAGS=-g -std=gnu99 -fPIC -O0
LDFLAGS= -L/usr/include/myclib -lmyclib -lmyos

%.o: %.c
	$(CC) -c $^ $(CFLAGS)

rtmp_client: $(OFILES) rtmp.h
	$(CC) -fPIC -o rtmp_client $(OFILES) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *.o rmtp_client

.PHONY: clean
