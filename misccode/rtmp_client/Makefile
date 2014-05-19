
CC=gcc
OFILES=amf0.o rtmp.o tcp.o url.o rtmp_client.o utils.o flv.o
CFLAGS=-g -std=gnu99 -fPIC -O0 -I/home/akuts/stuff/experiments/ffmpeg-0.6.7/build/include
LDFLAGS= -L/home/akuts/stuff/experiments/ffmpeg-0.6.7/build/lib -L/usr/include/myclib -lmyclib -lmyos -lpthread -lavformat -lavcodec -lavutil -lm -lz -lbz2 -lswscale

%.o: %.c
	$(CC) -c $^ $(CFLAGS)

rtmp_client: $(OFILES) rtmp.h
	$(CC) -fPIC -o rtmp_client $(OFILES) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *.o rmtp_client

.PHONY: clean
