CC := g++
CC_FLAGS := -g ${CFLAGS}

OBJS := Config.o ConnectionFactory.o QuorumPeer.o Logger.o Utils.o Follower.o
HEADER_FILES := Config.h ConnectionFactory.h QuorumPeer.h Logger.h \
				Utils.h Follower.h Leader.h Database.h Packet.h
LIBS := -lboost_thread -lboost_system

all: main

main: ${OBJS} ${HEADER_FILES}
	${CC} ${CC_FLAGS} -o $@ Main.cpp ${OBJS} ${LIBS}

%.o: %.cpp
	$(CC) $(CC_FLAGS) -o $@ -c $<

clean: 
	rm *.o main
