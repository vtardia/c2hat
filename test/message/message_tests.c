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

void TestMessage_getType() {

  // Testing error conditions
  assert(Message_getType("Untyped message") == 0);
  printf(".");
  assert(Message_getType("/o") == 0);
  printf(".");
  assert(Message_getType("") == 0);
  printf(".");
  assert(Message_getType(NULL) == 0);
  printf(".");

  // Testing OK type messages: may have an optional content
  assert(Message_getType("/ok Successful") == kMessageTypeOk);
  printf(".");
  assert(Message_getType("/ok") == kMessageTypeOk);
  printf(".");

  // Testing ERR type messages: must have a content
  assert(Message_getType("/err Invalid Something") == kMessageTypeErr);
  printf(".");
  assert(Message_getType("/err") == 0);
  printf(".");

  // Testing NICK type messages: must have a content
  assert(Message_getType("/nick Jason Foo") == kMessageTypeNick);
  printf(".");
  assert(Message_getType("/nick") == 0);
  printf(".");

  // Testing MSG type messages: must have a content
  assert(Message_getType("/msg This is a message") == kMessageTypeMsg);
  printf(".");
  assert(Message_getType("/msg") == 0);
  printf(".");

  // Testing QUIT type messages: may have an optional (ignored) content
  assert(Message_getType("/quit Something happened") == kMessageTypeQuit);
  printf(".");
  assert(Message_getType("/quit") == kMessageTypeQuit);
  printf(".");

  // Testing LOG type messages: must have a content
  assert(Message_getType("/log Something happened") == kMessageTypeLog);
  printf(".");
  assert(Message_getType("/log") == 0);
  printf(".");
}

void TestMessage_getContent() {
  char *message = NULL;
  char *content = NULL;
  size_t length = 0;

  // Test NULL everything
  content = Message_getContent(message, 0, length);
  assert(content == NULL);
  printf(".");

  // Test message shorter than the prefix
  message = "ok";
  length = 50;
  content = Message_getContent(message, kMessageTypeMsg, length);
  assert(content == NULL);
  printf(".");

  // Try to screw up with fake lengths
  message = "/msg Hello";
  length = 2;
  content = Message_getContent(message, kMessageTypeMsg, length);
  assert(content == NULL);
  printf(".");

  // Test correct trimming
  length = 8;
  content = Message_getContent(message, kMessageTypeMsg, length);
  assert(content != NULL);
  assert(strcmp(content, "He") == 0);
  Message_free(&content);
  printf(".");

  // Test unknown prefix
  message = "/foo Bar";
  length = 10;
  content = Message_getContent(message, kMessageTypeMsg, length);
  assert(content == NULL);
  printf(".");

  // Test unknown message type
  content = Message_getContent(message, 0, length);
  assert(content == NULL);
  printf(".");

  // Test message type nickname
  // In order to have a full 20 chars nickname, we need to add a pad
  // to the length: 5chars for the /nick prefix, + 1 space + null-terminator
  message = "/nick Joe";
  content = Message_getContent(message, kMessageTypeNick, kMaxNicknameLength + 7);
  assert(content != NULL);
  assert(strcmp(content, "Joe") == 0);
  Message_free(&content);
  printf(".");

  message = "/nick 01234567890123456789";
  content = Message_getContent(message, kMessageTypeNick, kMaxNicknameLength + 7);
  assert(content != NULL);
  assert(strcmp(content, "012345678901234") == 0);
  Message_free(&content);
  printf(".");

  // Test message type quit
  message = "/quit With a reason";
  content = Message_getContent(message, kMessageTypeQuit, 1024);
  assert(content != NULL);
  assert(strcmp(content, "With a reason") == 0);
  Message_free(&content);
  printf(".");

  message = "/quit"; // Without reason
  content = Message_getContent(message, kMessageTypeQuit, 1024);
  assert(content != NULL);
  assert(strcmp(content, "") == 0);
  Message_free(&content);
  printf(".");

  // Test message type ok
  message = "/ok With optional message";
  content = Message_getContent(message, kMessageTypeOk, 1024);
  assert(content != NULL);
  assert(strcmp(content, "With optional message") == 0);
  Message_free(&content);
  printf(".");

  message = "/ok"; // No optional message
  content = Message_getContent(message, kMessageTypeOk, 1024);
  assert(content != NULL);
  assert(strcmp(content, "") == 0);
  Message_free(&content);
  printf(".");

  // Test message type err
  message = "/err With required reason";
  content = Message_getContent(message, kMessageTypeErr, 1024);
  assert(content != NULL);
  assert(strcmp(content, "With required reason") == 0);
  Message_free(&content);
  printf(".");

  // Although we want to avoid error messages with no reason,
  // the minimum condition for success is != NULL.
  // The Message_getType will take care of validation pre-content
  message = "/err"; // No reason
  content = Message_getContent(message, kMessageTypeErr, 1024);
  assert(content != NULL);
  assert(strcmp(content, "") == 0);
  Message_free(&content);
  printf(".");

  // Test message type log
  message = "/log Something happened";
  content = Message_getContent(message, kMessageTypeLog, 1024);
  assert(content != NULL);
  assert(strcmp(content, "Something happened") == 0);
  Message_free(&content);
  printf(".");

  // Although we want to avoid log messages with no reason,
  // the minimum condition for success is != NULL.
  // The Message_getType will take care of validation pre-content
  message = "/log"; // No reason
  content = Message_getContent(message, kMessageTypeLog, 1024);
  assert(content != NULL);
  assert(strcmp(content, "") == 0);
  Message_free(&content);
  printf(".");
}

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
  assert(Message_getUser(message, user, length) == false);
  assert(strlen(user) == 0);
  printf(".");

  // Fails when there is no [
  message = "/msg SomeUser] did something";
  assert(Message_getUser(message, user, length) == false);
  assert(strlen(user) == 0);
  printf(".");

  // Fails when there is no ]
  message = "/msg [SomeUser did something";
  assert(Message_getUser(message, user, length) == false);
  assert(strlen(user) == 0);
  printf(".");

  // Fails when there is no []
  message = "/msg SomeUser did something";
  assert(Message_getUser(message, user, length) == false);
  assert(strlen(user) == 0);
  printf(".");

  // Fails when user length is <= 0
  message = "/msg [] did something";
  assert(Message_getUser(message, user, length) == false);
  assert(strlen(user) == 0);
  printf(".");

  message = "/msg ][ did something";
  assert(Message_getUser(message, user, length) == false);
  assert(strlen(user) == 0);
  printf(".");

  // Fails when user length is > max length
  length = 5;
  message = "/msg [Vercingetorix] said something";
  assert(Message_getUser(message, user, length) == false);
  assert(strlen(user) == 0);
  printf(".");

  // Returns a correct user
  length = 20;
  message = "/msg [Vercingetorix] said something";
  assert(Message_getUser(message, user, length));
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
  assert(Message_getUser(message, otherUser, sizeof(otherUser) -1) == false);
  printf(".");

  // Test a user name within a /quit message
  message = "/log [Joe24] just left the chat";
  assert(Message_getUser(message, user, length));
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
      ' ', 'M', 'y', ' ', 'n', 'a', 'm', 'e', ' ', 'i', 's', ' ', 'J', 'o', 'h', 'n', 0, // 32-48 = 17 chars
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
  assert(strlen(message->user) == 0);
  printf(".");
  assert(strncmp(message->content, "My name is John", kBufferSize) == 0);
  printf(".");
  C2HMessage_free(&message);
}
