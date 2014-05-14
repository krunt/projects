
CC=gcc
OFILES=rbtree.o logger.o hash.o utf8.o
CFLAGS=-g -std=gnu99 -fPIC

%.o: %.c
	$(CC) -c $^ $(CFLAGS)

myclib: $(OFILES)
	$(CC) -fPIC -shared -o libmyclib.so $(OFILES) $(CFLAGS)

install: myclib
	mkdir -p /usr/include/myclib
	mkdir -p /usr/lib/myclib
	cp libmyclib.so /usr/lib/
	cp -r *.h /usr/include/myclib

test: testlib.o myclib
	$(CC) -o testlib testlib.o -lmyclib

clean:
	rm -f *.o *.so testlib

.PHONY: clean install
