/*
 * Copyright (C) 2020 Vito Tardia
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "message.h"

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
}

void TestMessage_format() {
  // Dest, size or format shorter than prefix
  // size shorter than format + arguments + prefix
  printf(".");
}
