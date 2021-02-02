# Compiler base command and options
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c17
AR = ar rcs
LDFLAGS = -L lib
INCFLAGS = -I src/lib

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
	mkdir -p obj/server obj/client obj/lib lib bin

# Server final binary
server: prereq liblogger libsocket libpid obj/server/main.o obj/server/server.o
	$(CC) $(CFLAGS) obj/server/main.o obj/server/server.o $(LDFLAGS) -lpthread -llogger -lsocket -lpid -o bin/server


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

# INI parser
# obj/ini.o: ini/ini.c ini/ini.h
# 	$(CC) $(CFLAGS) -c ini/ini.c -o obj/ini.o $(OSFLAG) -DINI_ALLOW_MULTILINE=0


# Client final binary
client: prereq libsocket obj/client/main.o obj/client/client.o
	$(CC) $(CFLAGS) obj/client/*.o $(LDFLAGS) -lsocket -o bin/client


# Client dependencies

obj/client/main.o: src/client/main.c
	$(CC) $(CFLAGS) -c src/client/main.c $(INCFLAGS) -o obj/client/main.o $(OSFLAG)

obj/client/client.o: src/client/client.c
	$(CC) $(CFLAGS) -c src/client/client.c $(INCFLAGS) -o obj/client/client.o $(OSFLAG)


clean:
	rm -rfv bin/**
	rm -rfv obj/**
