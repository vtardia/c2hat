/**
 * Copyright (C) 2020-2022 Vito Tardia <https://vito.tardia.me>
 *
 * This file is part of C2Hat
 *
 * C2Hat is a simple client/server TCP chat written in C
 *
 * C2Hat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * C2Hat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with C2Hat. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "message/message.h"

// (private) Gets the user part of a message
bool Message_getUser(const char *message, C2HMessageType type, char *user, size_t length);

// (private) Gets the next available message from a message buffer
char *Message_get(MessageBuffer *buffer);

// (private) Frees memory space for a parsed message
void Message_free(char **);

// (private) Wraps an outgoing message into the given command type
void Message_format(unsigned int type, char *dest, size_t size, const char *format, ...);


// @TODO rewrite tests by testing the wrapper function
// void TestMessage_getType() {

//   // Testing error conditions
//   assert(Message_getType("Untyped message") == 0);
//   printf(".");
//   assert(Message_getType("/o") == 0);
//   printf(".");
//   assert(Message_getType("") == 0);
//   printf(".");
//   assert(Message_getType(NULL) == 0);
//   printf(".");

//   // Testing OK type messages: may have an optional content
//   assert(Message_getType("/ok Successful") == kMessageTypeOk);
//   printf(".");
//   assert(Message_getType("/ok") == kMessageTypeOk);
//   printf(".");

//   // Testing ERR type messages: must have a content
//   assert(Message_getType("/err Invalid Something") == kMessageTypeErr);
//   printf(".");
//   assert(Message_getType("/err") == 0);
//   printf(".");

//   // Testing NICK type messages: must have a content
//   assert(Message_getType("/nick Jason Foo") == kMessageTypeNick);
//   printf(".");
//   assert(Message_getType("/nick") == 0);
//   printf(".");

//   // Testing MSG type messages: must have a content
//   assert(Message_getType("/msg This is a message") == kMessageTypeMsg);
//   printf(".");
//   assert(Message_getType("/msg") == 0);
//   printf(".");

//   // Testing QUIT type messages: may have an optional (ignored) content
//   assert(Message_getType("/quit Something happened") == kMessageTypeQuit);
//   printf(".");
//   assert(Message_getType("/quit") == kMessageTypeQuit);
//   printf(".");

//   // Testing LOG type messages: must have a content
//   assert(Message_getType("/log Something happened") == kMessageTypeLog);
//   printf(".");
//   assert(Message_getType("/log") == 0);
//   printf(".");
// }

void TestMessage_format() {
  char message[1024] = {};

  // A simple successful message, without parameters
  // This will work even if message[1024] is left unitialised
  // even if it is bad practice
  Message_format(kMessageTypeMsg, message, 1024, "Hello World!");
  assert(message != NULL);
  assert(strcmp(message, "/msg Hello World!") == 0);
  printf(".");

  // Null destination message
  char *nullMessage = NULL;
  Message_format(kMessageTypeMsg, nullMessage, 1024, "Hello World!");
  assert(nullMessage == NULL);
  printf(".");

  // Dynamic char message
  // If the pointer is unitialised the compiler will issue a warning
  // and the behaviour is undefined.
  // However, using '-Werror=maybe-uninitialized' at compile time will catch the issue
  char *heapMessage = calloc(1024, 1);
  Message_format(kMessageTypeMsg, heapMessage, 1024, "Hello World!");
  assert(heapMessage != NULL);
  assert(strcmp(heapMessage, "/msg Hello World!") == 0);
  free(heapMessage);
  printf(".");

  // Dest, size or format shorter than prefix:
  // It works, the destination is written even if it's shorter than
  // the specified size or message.
  // There seems to be no buffer overflow or memory leaks although it's not normal
  // When size is shorter than format + arguments + prefix the message is simply truncated
  char shortMessage[4] = {};
  Message_format(kMessageTypeMsg, shortMessage, 1024, "Hello World!");
  assert(strcmp(shortMessage, "/msg Hello World!") == 0);
  printf(".");

  // Unknown prefix, an empty message should be returned
  memset(message, 0, 1024);
  Message_format(0, message, 1024, "Hello World!");
  assert(message != NULL);
  assert(strcmp(message, "") == 0);
  printf(".");

  // Use arguments
  memset(message, 0, 1024);
  char *nickname = "Fox";
  Message_format(kMessageTypeOk, message, 1024, "Hello %s!", nickname);
  assert(strcmp(message, "/ok Hello Fox!") == 0);
  printf(".");

  // OK with no arguments
  memset(message, 0, 1024);
  Message_format(kMessageTypeOk, message, 1024, "");
  assert(strcmp(message, "/ok") == 0);
  printf(".");
}

void TestMessage_getUser() {
  char *message = NULL;
  size_t length = 0;
  char user[21] = {};

  // Test that fails if type is not /msg or /log
  length = 20;
  message = "/err [SomeUser] did something";
  assert(Message_getUser(message, kMessageTypeErr, user, length) == false);
  assert(strlen(user) == 0);
  printf(".");

  // Fails when there is no [
  message = "/msg SomeUser] did something";
  assert(Message_getUser(message, kMessageTypeMsg, user, length) == false);
  assert(strlen(user) == 0);
  printf(".");

  // Fails when there is no ]
  message = "/msg [SomeUser did something";
  assert(Message_getUser(message, kMessageTypeMsg, user, length) == false);
  assert(strlen(user) == 0);
  printf(".");

  // Fails when there is no []
  message = "/msg SomeUser did something";
  assert(Message_getUser(message, kMessageTypeMsg, user, length) == false);
  assert(strlen(user) == 0);
  printf(".");

  // Fails when user length is <= 0
  message = "/msg [] did something";
  assert(Message_getUser(message, kMessageTypeMsg, user, length) == false);
  assert(strlen(user) == 0);
  printf(".");

  message = "/msg ][ did something";
  assert(Message_getUser(message, kMessageTypeMsg, user, length) == false);
  assert(strlen(user) == 0);
  printf(".");

  // Fails when user length is > max length
  length = 5;
  message = "/msg [Vercingetorix] said something";
  assert(Message_getUser(message, kMessageTypeMsg, user, length) == false);
  assert(strlen(user) == 0);
  printf(".");

  // Returns a correct user
  length = 20;
  message = "/msg [Vercingetorix] said something";
  assert(Message_getUser(message, kMessageTypeMsg, user, length));
  assert(strlen(user) == 13);
  assert(strncmp(user, "Vercingetorix", 13) == 0);
  printf(".");

  // Test a buffer overflow, when user[] is shorter than max length
  length = 5;
  // MUST be length + 1 (kMaxNicknameLength + 1) to avoid
  // unexpected buffer overflow results
  char otherUser[5] = {};
  message = "/msg [Abcde] said something";
  // Buffer too short to contain the username
  assert(Message_getUser(message, kMessageTypeMsg, otherUser, sizeof(otherUser) -1) == false);
  printf(".");

  // Test a user name within a /quit message
  message = "/log [Joe24] just left the chat";
  assert(Message_getUser(message, kMessageTypeMsg, user, length));
  assert(strlen(user) == 5);
  assert(strncmp(user, "Joe24", 5) == 0);
  printf(".");
}

void TestMessage_get() {
  MessageBuffer buffer = {
    .data = {
      '/', 'm', 's', 'g', ' ', 'H', 'e', 'l', 'l', 'o', 0, // 0-10 = 11 chars
      '/', 'm', 's', 'g', ' ', 'C', 'o', 'm', 'o', ' ', 'e', 's', 't', 'a', 's', '?', 0, // 11-27 = 17 chars
      '/', 'm', 's', 'g', // 28-31 = 4 chars
      ' ', 'M', 'y', ' ', 'n', 'a', 'm', 'e', ' ', 'i', 's', ' ', 'J', 'o', 'h', 'n', 0, // 32-48 = 17 chars
    }
  };
  char *message = NULL;

  // Test that the first message is read...
  message = Message_get(&buffer);
  assert(message != NULL);
  printf(".");

  assert(strncmp(message, "/msg Hello", strlen(message)) == 0);
  printf(".");

  assert(*buffer.start == '/');
  printf(".");

  // ...and properly cleaned
  Message_free(&message);
  assert(message == NULL);
  printf(".");

  // Test that the second message is read
  message = Message_get(&buffer);
  assert(message != NULL);
  printf(".");

  assert(strncmp(message, "/msg Como estas?", strlen(message)) == 0);
  printf(".");

  assert(*buffer.start == '/');
  printf(".");

  Message_free(&message);
  assert(message == NULL);
  printf(".");

  // Test that the third message is read
  message = Message_get(&buffer);
  assert(message != NULL);
  printf(".");

  assert(strncmp(message, "/msg My name is John", strlen(message)) == 0);
  printf(".");

  assert(*buffer.start == 0);
  printf(".");

  Message_free(&message);
  assert(message == NULL);
  printf(".");

  // Test that the no more messages are read
  message = Message_get(&buffer);
  assert(message == NULL);
  printf(".");

  assert(buffer.start == NULL);
  printf(".");
}

void TestC2HMessage_get() {
  MessageBuffer buffer = {
    .data = {
      '/', 'm', 's', 'g', ' ', 'H', 'e', 'l', 'l', 'o', 0, // 0-10 = 11 chars
      '/', 'm', 's', 'g', ' ', 'C', 'o', 'm', 'o', ' ', 'e', 's', 't', 'a', 's', '?', 0, // 11-27 = 17 chars
      '/', 'm', 's', 'g', // 28-31 = 4 chars
      ' ', '[', 'J', 'o', 'e', ']', ' ', 'I', ' ', 'a', 'm', ' ', 'J', 'o', 'h', 'n', 0, // 32-48 = 17 chars
    }
  };
  C2HMessage *message = NULL;

  message = C2HMessage_get(&buffer);
  assert(message->type == kMessageTypeMsg);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strncmp(message->content, "Hello", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  message = C2HMessage_get(&buffer);
  assert(message->type == kMessageTypeMsg);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strncmp(message->content, "Como estas?", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  message = C2HMessage_get(&buffer);
  assert(message->type == kMessageTypeMsg);
  printf(".");
  assert(strlen(message->user) == 3);
  printf(".");
  assert(strncmp(message->user, "Joe", kMaxNicknameSize) == 0);
  printf(".");
  assert(strncmp(message->content, "I am John", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  MessageBuffer buf2 = {
    .data = {
      '/', 'o', 'k', ' ', 'H', 'e', 'l', 'l', 'o', 0, // 0-9 = 10 chars
      '/', 'o', 'k', ' ', 0, // 10-14 = 5 chars
      '/', 'o', 'k', 0, // 15-18 = 4 chars
    }
  };

  message = C2HMessage_get(&buf2);
  assert(message->type == kMessageTypeOk);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strncmp(message->content, "Hello", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  message = C2HMessage_get(&buf2);
  assert(message->type == kMessageTypeOk);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strlen(message->content) == 0);
  printf(".");
  assert(strncmp(message->content, "", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  message = C2HMessage_get(&buf2);
  assert(message->type == kMessageTypeOk);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strlen(message->content) == 0);
  printf(".");
  assert(strncmp(message->content, "", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  MessageBuffer buf3 = {
    .data = {
      '/', 'q', 'u', 'i', 't', ' ', 'B', 'y', 'e', 0, // 0-9 = 10 chars
      '/', 'q', 'u', 'i', 't', ' ', 0, // 10-16 = 7 chars
      '/', 'q', 'u', 'i', 't', 0, // 17-22 = 6 chars
    }
  };

  message = C2HMessage_get(&buf3);
  assert(message->type == kMessageTypeQuit);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strncmp(message->content, "Bye", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  message = C2HMessage_get(&buf3);
  assert(message->type == kMessageTypeQuit);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strlen(message->content) == 0);
  printf(".");
  assert(strncmp(message->content, "", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  message = C2HMessage_get(&buf3);
  assert(message->type == kMessageTypeQuit);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strlen(message->content) == 0);
  printf(".");
  assert(strncmp(message->content, "", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);
}

void TestC2HMessage_create() {
  // Nick
  char *username = "JoePerry";
  C2HMessage *message = C2HMessage_create(kMessageTypeNick, "%s", username);
  assert(message->type == kMessageTypeNick);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strlen(message->content) == 8);
  printf(".");
  assert(strncmp(message->content, "JoePerry", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  // Error
  message = C2HMessage_create(kMessageTypeErr, "Authentication timeout expired!");
  assert(message->type == kMessageTypeErr);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strlen(message->content) == strlen("Authentication timeout expired!"));
  printf(".");
  assert(strncmp(message->content, "Authentication timeout expired!", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  // Simple OK
  message = C2HMessage_create(kMessageTypeOk, "");
  assert(message->type == kMessageTypeOk);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strlen(message->content) == 0);
  printf(".");
  assert(strncmp(message->content, "", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  // OK with message
  message = C2HMessage_create(kMessageTypeOk, "Hello %s!", username);
  assert(message->type == kMessageTypeOk);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strlen(message->content) == strlen("Hello JoePerry!"));
  printf(".");
  assert(strncmp(message->content, "Hello JoePerry!", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  // Log
  message = C2HMessage_create(kMessageTypeLog, "[%s] just joined the chat", username);
  assert(message->type == kMessageTypeLog);
  printf(".");
  assert(strlen(message->user) == 8);
  printf(".");
  assert(strncmp(message->user, "JoePerry", kMaxNicknameSize) == 0);
  printf(".");
  assert(strlen(message->content) == strlen("just joined the chat"));
  printf(".");
  assert(strncmp(message->content, "just joined the chat", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  // Simple Quit
  message = C2HMessage_create(kMessageTypeQuit, "");
  assert(message->type == kMessageTypeQuit);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strlen(message->content) == 0);
  printf(".");
  assert(strncmp(message->content, "", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  // Quit with reason
  message = C2HMessage_create(kMessageTypeQuit, "See you later!");
  assert(message->type == kMessageTypeQuit);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strlen(message->content) == strlen("See you later!"));
  printf(".");
  assert(strncmp(message->content, "See you later!", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  // Message (from client)
  message = C2HMessage_create(kMessageTypeMsg, "Hello folks, how are you doing?");
  assert(message->type == kMessageTypeMsg);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strlen(message->content) == strlen("Hello folks, how are you doing?"));
  printf(".");
  assert(strncmp(message->content, "Hello folks, how are you doing?", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  // Message (from server with username)
  message = C2HMessage_create(kMessageTypeMsg, "[JoePerry] Hello folks, how are you doing?");
  assert(message->type == kMessageTypeMsg);
  printf(".");
  assert(strlen(message->user) == 8);
  printf(".");
  assert(strncmp(message->user, "JoePerry", kMaxNicknameSize) == 0);
  printf(".");
  assert(strlen(message->content) == strlen("Hello folks, how are you doing?"));
  printf(".");
  assert(strncmp(message->content, "Hello folks, how are you doing?", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);
}

void TestC2HMessage_createFromString() {
  // in raw string out message
  C2HMessage *message = C2HMessage_createFromString("Hello, how are you?", strlen("Hello, how are you?"));
  assert(message->type == kMessageTypeMsg);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strlen(message->content) == strlen("Hello, how are you?"));
  printf(".");
  assert(strncmp(message->content, "Hello, how are you?", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  // in message, out message
  message = C2HMessage_createFromString("/msg Hello, how are you?", strlen("/msg Hello, how are you?"));
  assert(message->type == kMessageTypeMsg);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strlen(message->content) == strlen("Hello, how are you?"));
  printf(".");
  assert(strncmp(message->content, "Hello, how are you?", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  // in quit out quit
  message = C2HMessage_createFromString("/quit", strlen("/quit"));
  assert(message->type == kMessageTypeQuit);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strlen(message->content) == 0);
  printf(".");
  assert(strncmp(message->content, "", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  // in nick out nick
  message = C2HMessage_createFromString("/nick Joe24", strlen("/nick Joe24"));
  assert(message->type == kMessageTypeNick);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strlen(message->content) == 5);
  printf(".");
  assert(strncmp(message->content, "Joe24", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  // in unrecognised command out message wrap
  message = C2HMessage_createFromString("/help", strlen("/help"));
  assert(message->type == kMessageTypeMsg);
  printf(".");
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strlen(message->content) == strlen("/help"));
  printf(".");
  assert(strncmp(message->content, "/help", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);

  // in log out null
  message = C2HMessage_createFromString("/log Something", strlen("/log Something"));
  assert(message == NULL);
  printf(".");

  // in ok out null
  message = C2HMessage_createFromString("/ok", strlen("/ok"));
  assert(message == NULL);
  printf(".");

  // in err out null
  message = C2HMessage_createFromString("/err Some error", strlen("/err Some error"));
  assert(message == NULL);
  printf(".");

  message = C2HMessage_createFromString("", 0);
}
