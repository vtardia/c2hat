/*
 * Copyright (C) 2021 Vito Tardia
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "message/message.h"
#include "../../src/client/uilog.h"

int main() {
  char *buffer = NULL;
  size_t length = 0;
  ChatLogEntry *entry = NULL;

  // Test a log message
  buffer = "/log [Joe24] just left the chat";
  // Length is the number of bytes received from the network,
  // so it will include the NULL terminator
  length = strlen(buffer) + 1;
  entry = ChatLogEntry_create(buffer, length);
  assert(entry->type == kMessageTypeLog);
  printf(".");
  assert(strcmp(entry->username, "Joe24") == 0);
  printf(".");
  assert(strcmp(entry->content, "[Joe24] just left the chat") == 0);
  printf(".");
  ChatLogEntry_free(&entry);

  // Test a chat message
  buffer = "/msg [Joe24] Hello world!";
  length = strlen(buffer) + 1;
  entry = ChatLogEntry_create(buffer, length);
  assert(entry->type == kMessageTypeMsg);
  printf(".");
  assert(strcmp(entry->username, "Joe24") == 0);
  printf(".");
  assert(strcmp(entry->content, "[Joe24] Hello world!") == 0);
  printf(".");
  ChatLogEntry_free(&entry);

  printf("\n");
  return EXIT_SUCCESS;
}

