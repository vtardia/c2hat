# Compiler base command and options
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c17

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
server: prereq obj/server/main.o obj/server/server.o obj/logger.o obj/socket.o obj/server/pid.o
	$(CC) $(CFLAGS) obj/server/main.o obj/logger.o obj/socket.o obj/server/server.o obj/server/pid.o -o bin/server


# Server dependencies

obj/server/main.o: server/main.c
	$(CC) $(CFLAGS) -c server/main.c -I socket -I logger -o obj/server/main.o $(OSFLAG)

obj/server/server.o: server/server.c
	$(CC) $(CFLAGS) -c server/server.c -I socket -I logger -o obj/server/server.o $(OSFLAG)

obj/server/pid.o: server/pid.c
	$(CC) $(CFLAGS) -c server/pid.c -I logger -o obj/server/pid.o $(OSFLAG)

obj/socket.o: socket/socket.c socket/socket.h
	$(CC) $(CFLAGS) -c socket/socket.c -I logger -o obj/socket.o $(OSFLAG)

obj/logger.o: logger/logger.c logger/logger.h
	$(CC) $(CFLAGS) -c logger/logger.c -o obj/logger.o $(OSFLAG)

# obj/ini.o: ini/ini.c ini/ini.h
# 	$(CC) $(CFLAGS) -c ini/ini.c -o obj/ini.o $(OSFLAG) -DINI_ALLOW_MULTILINE=0


# Client final binary
client: obj/client/main.o
	$(CC) $(CFLAGS) obj/client/main.o -o bin/client


# Client dependencies

obj/client/main.o: client/main.c
	$(CC) $(CFLAGS) -c client/main.c -o obj/client/main.o $(OSFLAG)


clean:
	rm -rfv bin/**
	rm -rfv obj/**
