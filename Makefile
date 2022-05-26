# Compiler base command and options
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c17 -O3 -I src/lib

# Linker options
# Notes:
#  - You need to install libssl-dev in order to compile with TLS support
#  - On macOS you need to install the updated ncurses with Homebrew,
#    then you can use $(ncursesw6-config --cflags --libs)
#    to get the correct parameters
LDFLAGS = -L lib
LDLIBS = -lpthread -lssl -lcrypto
SERVERLIBS =
CLIENTLIBS = -ldl -lncursesw
TESTCONFIGLIBS =

# Default prefix for executable files
BINPREFIX = c2hat-

# Test prefix for Valgrind (currently Linux only)
VALGRIND = valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes

# Default Hash size
HASH_SIZE = 128

# OS detection
OSFLAG := -D_XOPEN_SOURCE_EXTENDED
ifeq ($(OS), Windows_NT)
	OSFLAG += -D WIN32
	VALGRIND =
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		OSFLAG += -D LINUX -D _GNU_SOURCE -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600
		SERVERLIBS += -lrt
		TESTCONFIGLIBS +=  -lrt
		CLIENTLIBS += -L/usr/lib -ltinfo
	endif
	ifeq ($(UNAME_S),Darwin)
		OSFLAG += -D MACOS -D_XOPEN_CURSES -D_DARWIN_C_SOURCE
		VALGRIND =
		CFLAGS += -I$(HOMEBREW_CELLAR)/ncurses/6.3/include/ncursesw \
			-I$(HOMEBREW_CELLAR)/ncurses/6.3/include \
			-I$(HOMEBREW_CELLAR)/openssl@1.1/1.1.1n/include
		LDFLAGS += -L$(HOMEBREW_CELLAR)/openssl@1.1/1.1.1n/lib
		CLIENTLIBS += -L$(HOMEBREW_CELLAR)/ncurses/6.3/lib -Wl,-search_paths_first \
			-L$(HOMEBREW_CELLAR)/openssl@1.1/1.1.1n/lib
	endif
endif

# Replace 'src/server/<filename>.c' with 'server/<filename>',
# for each element whose relative path matches 'src/server/*.c' 😎
SERVER_OBJECTS = $(patsubst src/server/%.c,server/%,$(wildcard src/server/*.c))
CLIENT_OBJECTS = $(patsubst src/client/%.c,client/%,$(wildcard src/client/*.c))

COMMON_LIBRARIES = logger socket list queue message
SERVER_LIBRARIES = config validate ini
CLIENT_LIBRARIES = hash wtrim

# Targets

all: prereq client server

debug: clean prereq/debug all

prereq:
	mkdir -p obj/server obj/client obj/lib obj/lib/server obj/lib/client lib bin

prereq/debug:
	$(eval CFLAGS += -g)

prereq/tests:
	$(eval CFLAGS += -DTest_operations)
	mkdir -p bin/test

# Server final binary
server: prereq $(COMMON_LIBRARIES) $(SERVER_LIBRARIES) $(SERVER_OBJECTS)
	$(CC) $(CFLAGS) obj/server/*.o obj/lib/*.o obj/lib/server/*.o \
		$(LDFLAGS) $(LDLIBS) $(SERVERLIBS) -o bin/$(BINPREFIX)server

# Client final binary
client: prereq $(COMMON_LIBRARIES) $(CLIENT_LIBRARIES) $(CLIENT_OBJECTS)
	$(CC) $(CFLAGS) obj/client/*.o obj/lib/*.o obj/lib/client/*.o \
		$(LDFLAGS) $(LDLIBS) $(CLIENTLIBS) -o bin/$(BINPREFIX)cli

# Common dependencies
$(COMMON_LIBRARIES):
	$(CC) $(CFLAGS) -c src/lib/$@/$@.c $(OSFLAG) -o obj/lib/$@.o

# Server dependencies
$(SERVER_OBJECTS):
	$(CC) $(CFLAGS) -c src/$@.c $(OSFLAG) -o obj/$@.o

$(SERVER_LIBRARIES):
	$(CC) $(CFLAGS) -DINI_ALLOW_MULTILINE=0 -c src/lib/$@/$@.c $(OSFLAG) -o obj/lib/server/$@.o

# Client dependencies
$(CLIENT_OBJECTS):
	$(CC) $(CFLAGS) -c src/$@.c $(OSFLAG) -o obj/$@.o

$(CLIENT_LIBRARIES):
	$(CC) $(CFLAGS) -c src/lib/$@/$@.c $(OSFLAG) -o obj/lib/client/$@.o

# INI parser
# obj/ini.o: ini/ini.c ini/ini.h
# 	$(CC) $(CFLAGS) -c ini/ini.c -o obj/ini.o $(OSFLAG) -DINI_ALLOW_MULTILINE=0

# Test Bot
bot: prereq $(COMMON_LIBRARIES)
	mkdir -p bin/test
	$(CC) $(CFLAGS) -I src/client \
		test/bot/main.c src/client/client.c obj/lib/*.o \ # logger, socket, message
		$(OSFLAG) $(LDFLAGS) $(LDLIBS) -o bin/test/bot

# Unit test targets
test: clean prereq/debug test/list test/queue test/message test/logger test/config test/validate

test/hash: prereq/tests
	$(CC) -g $(CFLAGS) test/hash/*.c src/lib/hash/*.c $(OSFLAG) $(LDFLAGS) -o bin/test/hash
	$(VALGRIND) bin/test/hash

test/list: prereq/tests
	$(CC) -g $(CFLAGS) test/list/*.c src/lib/list/*.c $(OSFLAG) $(LDFLAGS) -o bin/test/list
	$(VALGRIND) bin/test/list

test/queue: prereq/tests
	$(CC) -g $(CFLAGS) test/queue/*.c src/lib/queue/*.c $(OSFLAG) $(LDFLAGS) -o bin/test/queue
	$(VALGRIND) bin/test/queue

test/message: prereq/tests
	$(CC) -g $(CFLAGS) -I src/server $(OSFLAG) test/message/*.c src/lib/message/*.c \
		$(LDFLAGS) -o bin/test/message
	$(VALGRIND) bin/test/message

test/logger: prereq/tests
	$(CC) -g $(CFLAGS) -o bin/test/logger $(OSFLAG) -lpthread src/lib/logger/logger.c
	$(VALGRIND) bin/test/logger

test/pid: prereq/debug
	mkdir -p bin/test
	$(CC) $(CFLAGS) -I src/server \
		test/pid/*.c src/server/pid.c src/lib/logger/*.c \
		$(LDFLAGS) -o bin/test/pid $(OSFLAG) \
		&& cp test/pid/pid.sh bin/test/pid.sh \
		&& ./bin/test/pid.sh \
		&& echo "Done!"

test/config: prereq/tests
	$(CC) -g $(CFLAGS) test/config/*.c src/lib/config/*.c \
		$(OSFLAG) $(LDFLAGS) $(TESTCONFIGLIBS) -o bin/test/config
	$(VALGRIND) bin/test/config

test/validate: prereq/tests
	$(CC) -g $(CFLAGS) $(OSFLAG) test/validate/*.c src/lib/validate/*.c \
		$(LDFLAGS) -o bin/test/validate
	$(VALGRIND) bin/test/validate

test/uilog: prereq/tests
	$(CC) -g $(CFLAGS) $(OSFLAG) test/uilog/*.c src/lib/message/*.c src/client/uilog.c \
		$(LDFLAGS) -o bin/test/uilog
	$(VALGRIND) bin/test/uilog

clean:
	rm -rfv bin/**
	rm -rfv obj/**
	rm -rfv lib/**
	rm -rfv docs/**

.PHONY: docs
docs: docs Doxyfile
	mkdir -p docs
	doxygen Doxyfile
