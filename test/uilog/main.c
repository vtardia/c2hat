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

