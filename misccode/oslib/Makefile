
CC_UNIX=gcc
CC_WIN=i586-mingw32msvc-gcc

HAVE_WIN = $(shell which i586-mingw32msvc-gcc >/dev/null 2>&1 && echo "1")

OUT_UNIX=libmyos.so
OUT_WIN=libmyos.dll

CFLAGS=-g -fPIC

all: myos_unix myos_win

myos.o: myos.c
	$(CC_UNIX) $(CFLAGS) -c $^ -o myos_unix.o
	$(CC_UNIX) $(CFLAGS) -c myos_unix.c -o myos_unix1.o
ifeq ($(HAVE_WIN), 1)
	$(CC_WIN) $(CFLAGS) -c $^ -o myos_win.o
	$(CC_WIN) $(CFLAGS) -c myos_win32.c -o myos_win2.o
endif

myos_unix: myos.o
	$(CC_UNIX) $(CFLAGS) -shared -o $(OUT_UNIX) myos_unix.o myos_unix1.o 

myos_win: myos.o
ifeq ($(HAVE_WIN), 1)
	$(CC_WIN) $(CFLAGS) -shared -o $(OUT_WIN) myos_win.o myos_win2.o -lws2_32
else
	@echo "stub" >/dev/null
endif

test: myos_test.c myos_unix myos_win
	$(CC_UNIX) $(CFLAGS) -o myostest_unix $< -I. -lmyos
ifeq ($(HAVE_WIN), 1)
	$(CC_WIN) -DWIN32 $(CFLAGS) -o myostest_win.exe $< -I. -lmyos
endif

install: myos_unix myos_win
	/usr/bin/install -m 0755 myos.h /usr/include
	/usr/bin/install -m 0755 myos_unix.h /usr/include
	/usr/bin/install -m 0755 libmyos.so /usr/lib
ifeq ($(HAVE_WIN), 1)
	/usr/bin/install -m 0755 myos_win32.h /usr/include
	/usr/bin/install -m 0755 libmyos.dll /usr/lib
endif
	
clean:
	rm -f *.o *.so *.dll *.exe myostest_unix
