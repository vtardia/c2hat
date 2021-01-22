# Compiler base command and options
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c17
AR = ar rcs
LDFLAGS = -Lbin

# OS detection
OSFLAG :=
ifeq ($(OS), Windows_NT)
	OSFLAG += -D WIN32
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		OSFLAG += -D LINUX -D _GNU_SOURCE
	endif
	ifeq ($(UNAME_S),Darwin)
		OSFLAG += -D MACOS
	endif
endif

all: prereq client server

prereq:
	mkdir -p obj/server obj/client bin

# Server final binary
server: prereq liblogger libsocket libpid obj/server/main.o obj/server/server.o
	$(CC) $(CFLAGS) obj/server/main.o obj/server/server.o $(LDFLAGS) -lpthread -llogger -lsocket -lpid -o bin/server


# Server dependencies

obj/server/main.o: server/main.c
	$(CC) $(CFLAGS) -c server/main.c -I socket -I logger -o obj/server/main.o $(OSFLAG)

obj/server/server.o: server/server.c
	$(CC) $(CFLAGS) -c server/server.c -I socket -I logger -o obj/server/server.o $(OSFLAG)


# PID static library
obj/server/pid.o: server/pid.c
	$(CC) $(CFLAGS) -c server/pid.c -I logger -o obj/server/pid.o $(OSFLAG)

libpid: obj/server/pid.o
	$(AR) bin/libpid.a obj/server/pid.o

# Socket static library
obj/socket.o: socket/socket.c socket/socket.h
	$(CC) $(CFLAGS) -c socket/socket.c -I logger -o obj/socket.o $(OSFLAG)

libsocket: obj/socket.o
	$(AR) bin/libsocket.a obj/socket.o


# Logger static library
obj/logger.o: logger/logger.c logger/logger.h
	$(CC) $(CFLAGS) -c logger/logger.c -o obj/logger.o $(OSFLAG)

liblogger: obj/logger.o
	$(AR) bin/liblogger.a obj/logger.o

# INI parser
# obj/ini.o: ini/ini.c ini/ini.h
# 	$(CC) $(CFLAGS) -c ini/ini.c -o obj/ini.o $(OSFLAG) -DINI_ALLOW_MULTILINE=0


# Client final binary
client: prereq libsocket obj/client/main.o obj/client/client.o
	$(CC) $(CFLAGS) obj/client/*.o $(LDFLAGS) -lsocket -o bin/client


# Client dependencies

obj/client/main.o: client/main.c
	$(CC) $(CFLAGS) -c client/main.c -I socket -o obj/client/main.o $(OSFLAG)

obj/client/client.o: client/client.c
	$(CC) $(CFLAGS) -c client/client.c -I socket -o obj/client/client.o $(OSFLAG)


clean:
	rm -rfv bin/**
	rm -rfv obj/**
