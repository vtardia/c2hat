# Compiler base command and options
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c17
AR = ar rcs
LDFLAGS = -L lib
INCFLAGS = -I src/lib
VALGRIND = valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes
SERVERLIBS = -lpthread -llogger -lsocket -lpid -lqueue -llist -lmessage -lconfig
TESTCONFIGLIBS =
BINPREFIX = c2hat-

# OS detection
OSFLAG :=
ifeq ($(OS), Windows_NT)
	OSFLAG += -D WIN32
	VALGRIND =
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		OSFLAG += -D LINUX -D _GNU_SOURCE
		SERVERLIBS +=  -lrt
		TESTCONFIGLIBS +=  -lrt
	endif
	ifeq ($(UNAME_S),Darwin)
		OSFLAG += -D MACOS
		VALGRIND =
	endif
endif

all: prereq client server

debug: clean prereq/debug all

prereq:
	mkdir -p obj/server obj/client obj/lib obj/test lib bin

prereq/debug:
	$(eval CFLAGS += -g)

# Server final binary
server: prereq liblogger libsocket libpid liblist libqueue libmessage libconfig obj/server/main.o obj/server/server.o
	$(CC) $(CFLAGS) obj/server/main.o obj/server/server.o $(LDFLAGS) $(SERVERLIBS) -o bin/$(BINPREFIX)server


# Server dependencies

obj/server/main.o: src/server/main.c
	$(CC) $(CFLAGS) -c src/server/main.c $(INCFLAGS) -o obj/server/main.o $(OSFLAG)

obj/server/server.o: src/server/server.c
	$(CC) $(CFLAGS) -c src/server/server.c $(INCFLAGS) -o obj/server/server.o $(OSFLAG)


# PID static library
obj/server/pid.o: src/server/pid.c
	$(CC) $(CFLAGS) -c src/server/pid.c $(INCFLAGS) -o obj/server/pid.o $(OSFLAG)

libpid: prereq obj/server/pid.o
	$(AR) lib/libpid.a obj/server/pid.o

# Message static library
obj/lib/message.o: src/lib/message/message.c
	$(CC) $(CFLAGS) -c src/lib/message/message.c $(INCFLAGS) -o obj/lib/message.o $(OSFLAG)

libmessage: prereq obj/lib/message.o
	$(AR) lib/libmessage.a obj/lib/message.o

# Socket static library
obj/lib/socket.o: src/lib/socket/socket.c
	$(CC) $(CFLAGS) -c src/lib/socket/socket.c $(INCFLAGS) -o obj/lib/socket.o $(OSFLAG)

libsocket: prereq obj/lib/socket.o
	$(AR) lib/libsocket.a obj/lib/socket.o

# Logger static library
obj/lib/logger.o: src/lib/logger/logger.c
	$(CC) $(CFLAGS) -c src/lib/logger/logger.c -o obj/lib/logger.o $(OSFLAG)

liblogger: prereq obj/lib/logger.o
	$(AR) lib/liblogger.a obj/lib/logger.o

# List static library
obj/lib/list.o: src/lib/list/list.c
	$(CC) $(CFLAGS) -c src/lib/list/list.c -o obj/lib/list.o $(OSFLAG)

liblist: prereq obj/lib/list.o
	$(AR) lib/liblist.a obj/lib/list.o

# Queue static library
obj/lib/queue.o: src/lib/queue/queue.c
	$(CC) $(CFLAGS) -c src/lib/queue/queue.c -o obj/lib/queue.o $(OSFLAG)

libqueue: prereq obj/lib/queue.o
	$(AR) lib/libqueue.a obj/lib/queue.o

# Config static library
obj/lib/config.o: src/lib/config/config.c
	$(CC) $(CFLAGS) -c src/lib/config/config.c -o obj/lib/config.o $(OSFLAG)

libconfig: prereq obj/lib/config.o
	$(AR) lib/libconfig.a obj/lib/config.o

# INI parser
# obj/ini.o: ini/ini.c ini/ini.h
# 	$(CC) $(CFLAGS) -c ini/ini.c -o obj/ini.o $(OSFLAG) -DINI_ALLOW_MULTILINE=0


# Client final binary
client: prereq libsocket libmessage obj/client/app.o obj/client/main.o obj/client/client.o obj/client/ui.o
	$(CC) $(CFLAGS) obj/client/*.o $(LDFLAGS) -lsocket -lpthread -lmessage -lncurses -o bin/$(BINPREFIX)cli


# Client dependencies

obj/client/main.o: src/client/main.c
	$(CC) $(CFLAGS) -c src/client/main.c $(INCFLAGS) -o obj/client/main.o $(OSFLAG)

obj/client/app.o: src/client/app.c
	$(CC) $(CFLAGS) -c src/client/app.c $(INCFLAGS) -o obj/client/app.o $(OSFLAG)

obj/client/client.o: src/client/client.c
	$(CC) $(CFLAGS) -c src/client/client.c $(INCFLAGS) -o obj/client/client.o $(OSFLAG)

obj/client/ui.o: src/client/ui.c
	$(CC) $(CFLAGS) -c src/client/ui.c $(INCFLAGS) -o obj/client/ui.o $(OSFLAG)

# Test Bot
bot: prereq libsocket libmessage obj/test/bot/main.o obj/client/client.o
	mkdir -p bin/test
	$(CC) $(CFLAGS) obj/test/bot/main.o obj/client/client.o $(LDFLAGS) -lsocket -lpthread -lmessage -o bin/test/bot

obj/test/bot/main.o: test/bot/main.c
	mkdir -p obj/test/bot bin/test
	$(CC) $(CFLAGS) -c test/bot/main.c $(INCFLAGS) -I src/client -o obj/test/bot/main.o $(OSFLAG)

# Unit test targets
test: clean prereq/debug test/list test/queue test/message test/logger test/config

test/list: prereq/debug liblist
	mkdir -p obj/test/list bin/test
	$(CC) $(CFLAGS) -c test/list/list_tests.c $(INCFLAGS) -o obj/test/list/list_tests.o $(OSFLAG)
	$(CC) $(CFLAGS) -c test/list/main.c $(INCFLAGS) -o obj/test/list/main.o $(OSFLAG)
	$(CC) $(CFLAGS) obj/test/list/main.o obj/test/list/list_tests.o $(LDFLAGS) -llist -o bin/test/list
	$(VALGRIND) bin/test/list

test/queue: prereq/debug libqueue
	mkdir -p obj/test/queue bin/test
	$(CC) $(CFLAGS) -c test/queue/queue_tests.c $(INCFLAGS) -o obj/test/queue/queue_tests.o $(OSFLAG)
	$(CC) $(CFLAGS) -c test/queue/main.c $(INCFLAGS) -o obj/test/queue/main.o $(OSFLAG)
	$(CC) $(CFLAGS) obj/test/queue/main.o obj/test/queue/queue_tests.o $(LDFLAGS) -lqueue -o bin/test/queue
	$(VALGRIND) bin/test/queue

test/message: prereq/debug libmessage
	mkdir -p obj/test/message bin/test
	$(CC) $(CFLAGS) -c test/message/message_tests.c $(INCFLAGS) -I src/server -o obj/test/message/message_tests.o $(OSFLAG)
	$(CC) $(CFLAGS) -c test/message/main.c $(INCFLAGS) -I src/server -o obj/test/message/main.o $(OSFLAG)
	$(CC) $(CFLAGS) obj/test/message/main.o obj/test/message/message_tests.o $(LDFLAGS) -lmessage -o bin/test/message
	$(VALGRIND) bin/test/message

test/logger: prereq/debug liblogger
	mkdir -p obj/test/logger bin/test
	$(CC) $(CFLAGS) -c test/logger/main.c $(INCFLAGS) -o obj/test/logger/main.o $(OSFLAG)
	$(CC) $(CFLAGS) obj/test/logger/main.o $(LDFLAGS) -llogger -o bin/test/logger
	$(VALGRIND) bin/test/logger

test/pid: prereq/debug libpid liblogger
	mkdir -p obj/test/pid bin/test
	$(CC) $(CFLAGS) -c test/pid/main.c $(INCFLAGS) -I src/server -o obj/test/pid/main.o $(OSFLAG)
	$(CC) $(CFLAGS) obj/test/pid/main.o $(LDFLAGS) -lpid -llogger -o bin/test/pid
	cp test/pid/pid.sh bin/test/pid.sh
	./bin/test/pid.sh

test/config: prereq/debug libconfig
	mkdir -p obj/test/config bin/test
	$(CC) $(CFLAGS) -c test/config/main.c $(INCFLAGS) -o obj/test/config/main.o $(OSFLAG)
	$(CC) $(CFLAGS) obj/test/config/main.o $(LDFLAGS) -lconfig $(TESTCONFIGLIBS) -o bin/test/config
	$(VALGRIND) bin/test/config

clean:
	rm -rfv bin/**
	rm -rfv obj/**
	rm -rfv lib/**
	rm -rfv docs/**

.PHONY: docs
docs: docs Doxyfile
	mkdir -p docs
	doxygen Doxyfile
