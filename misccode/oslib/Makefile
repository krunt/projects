
CC_UNIX=gcc
CC_WIN=i586-mingw32msvc-gcc

OUT_UNIX=libmyos.so
OUT_WIN=libmyos.dll

CFLAGS=-g -fPIC

all: myos_unix myos_win

myos.o: myos.c
	$(CC_UNIX) $(CFLAGS) -c $^ -o myos_unix.o
	$(CC_UNIX) $(CFLAGS) -c myos_unix.c -o myos_unix1.o
	$(CC_WIN) $(CFLAGS) -c $^ -o myos_win.o
	$(CC_WIN) $(CFLAGS) -c myos_win32.c -o myos_win2.o

myos_unix: myos.o
	$(CC_UNIX) $(CFLAGS) -shared -o $(OUT_UNIX) myos_unix.o myos_unix1.o 

myos_win: myos.o
	$(CC_WIN) $(CFLAGS) -shared -o $(OUT_WIN) myos_win.o myos_win2.o

test: myos_test.c myos_unix myos_win
	$(CC_UNIX) $(CFLAGS) -o myostest_unix $< -I$(call pwd) -lmyos_unix
	$(CC_WIN) -DWIN32 $(CFLAGS) -o myostest_win.exe $< -I$(call pwd) -lmyos_win
	
clean:
	rm -f *.o *.so *.dll *.exe myostest_unix
