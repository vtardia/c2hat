# Base command
CC = gcc -Wall -Wextra -Werror -std=c17

# target (ie file): dependency list
all: prereq client server

prereq:
	mkdir -p obj/server obj/client bin

server: prereq obj/server/main.o obj/server/server.o obj/logger.o obj/socket.o
	$(CC) obj/server/main.o obj/logger.o obj/socket.o obj/server/server.o -o bin/server


obj/server/main.o: server/main.c
	$(CC) -c server/main.c -I socket -I logger -o obj/server/main.o

obj/server/server.o: server/server.c
	$(CC) -c server/server.c -I socket -I logger -o obj/server/server.o

obj/socket.o: socket/socket.c socket/socket.h
	$(CC) -c socket/socket.c -I logger -o obj/socket.o

obj/logger.o: logger/logger.c logger/logger.h
	$(CC) -c logger/logger.c -o obj/logger.o


client: obj/client/main.o
	$(CC) obj/client/main.o -o bin/client

obj/client/main.o: client/main.c
	$(CC) -c client/main.c -o obj/client/main.o


clean:
	rm -rfv bin/**
	rm -rfv obj/**
