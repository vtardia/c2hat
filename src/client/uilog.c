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

#ifndef __STDC_WANT_LIB_EXT1__
  // Ensure memset_s() is available
  #define __STDC_WANT_LIB_EXT1__ 1
#endif

#ifdef __linux__
  #define memset_s(s, smax, c, n) explicit_bzero(s, n)
#endif

#include "uilog.h"
#include "message/message.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Creates a new log entry from the given data
 */
ChatLogEntry *ChatLogEntry_create(char *buffer, size_t length) {
  ChatLogEntry *entry = calloc(sizeof(ChatLogEntry), 1);
  if (entry != NULL) {
    // Get local time
    time_t now = time(NULL);
    int result = strftime(entry->timestamp, 15, "%H:%M:%S", localtime(&now));
    if (result <= 0) memset(entry->timestamp, 0, 15);

    // Get entry type
    entry->type = Message_getType(buffer);

    // Get message content and length
    char *content = Message_getContent(buffer, entry->type, length);
    strncpy(entry->content, content, kBroadcastBufferSize - 1);
    entry->length = strlen(entry->content);
    Message_free(&content);

    // If the message is empty (e.g /ok with no detail), cleanup and return NULL
    if (entry->length == 0) {
      // Paranoid free
      memset_s(entry->content, kBroadcastBufferSize, 0, kBroadcastBufferSize);
      ChatLogEntry_free(&entry);
      return NULL;
    }

    // Extract the user name
    Message_getUser(buffer, entry->username, kMaxNicknameSize);
    return entry;
  }
  return NULL;
}

/**
 * Cleanup memory for a log entry
 */
void ChatLogEntry_free(ChatLogEntry **entry) {
  if (entry != NULL) {
    // Erase the item
    memset_s(*entry, sizeof(ChatLogEntry), 0, sizeof(ChatLogEntry));
    // Free the pointer to which entry is pointing
    // which is the actual pointer to the object
    free(*entry);
    *entry = NULL;
  }
}

