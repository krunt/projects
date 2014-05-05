
CC_UNIX=gcc
CC_WIN=i586-mingw32msvc-gcc

OUT_UNIX=libmyos.so
OUT_WIN=libmyos.dll

CFLAGS=-g -fPIC

myos.o: myos.c
	$(CC_UNIX) $(CFLAGS) -c $^ -o myos_unix.o
	$(CC_WIN) $(CFLAGS) -c $^ -o myos_win.o

all: myos_unix myos_win

myos_unix: myos.o
	$(CC_UNIX) $(CFLAGS) -shared -o $(OUT_UNIX) myos_unix.o 

myos_win: myos.o
	$(CC_WIN) $(CFLAGS) -shared -o $(OUT_WIN) myos_win.o
	
clean:
	rm -f *.o *.so *.dll
