
CC=g++
CCFLAGS=-g
LFLAGS=-lboost_thread -lboost_date_time

main: blockcache file_operations
	${CC} ${CCFLAGS} -o main main.cc blockcache.o file_operations.o ${LFLAGS}

blockcache:
	${CC} ${CCFLAGS} -c blockcache.cc

file_operations:
	${CC} ${CCFLAGS} -c file_operations.cc
