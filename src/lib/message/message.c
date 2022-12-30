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

#include "message.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "trim/trim.h"

static const char kMessageTypePrefixMsg[]  = "/msg";
static const char kMessageTypePrefixNick[] = "/nick";
static const char kMessageTypePrefixQuit[] = "/quit"; // Optional trailing space/content
static const char kMessageTypePrefixLog[]  = "/log";
static const char kMessageTypePrefixErr[]  = "/err";
static const char kMessageTypePrefixOk[]   = "/ok";   // Optional trailing space/content

/**
 * Finds the type of a given message
 * @param[in] message The server or client message content
 * @param[out] The message type code or 0 on failure
 */
int Message_getType(const char *message) {
  // sizeof may be faster than strlen because it is known at compile time
  // but it includes the NULL terminator, so we need -1
  #define MATCH(prefix) (strncmp(message, prefix, sizeof(prefix) - 1) == 0)
  #define SPACE_AFTER(prefix) (*(message + sizeof(prefix) -1) == ' ')
  if (message != NULL) {
    if (MATCH(kMessageTypePrefixNick) && SPACE_AFTER(kMessageTypePrefixNick)) {
      return kMessageTypeNick;
    }
    if (MATCH(kMessageTypePrefixMsg) && SPACE_AFTER(kMessageTypePrefixMsg)) {
      return kMessageTypeMsg;
    }
    if (MATCH(kMessageTypePrefixQuit)) {
      // Trailing space and additional content is optional
      return kMessageTypeQuit;
    }
    if (MATCH(kMessageTypePrefixLog) && SPACE_AFTER(kMessageTypePrefixLog)) {
      return kMessageTypeLog;
    }
    if (MATCH(kMessageTypePrefixErr) && SPACE_AFTER(kMessageTypePrefixErr)) {
      return kMessageTypeErr;
    }
    if (MATCH(kMessageTypePrefixOk)) {
      // Trailing space and additional content is optional
      return kMessageTypeOk;
    }
  }
  return 0;
}

/**
 * Finds the user name of a given message
 * The message type must be /msg
 * @param[in] message The server or client message content
 * @param[in] user    Contains the extracted user name
 * @param[in] length The maximum length of the returned content
 */
bool Message_getUser(const char *message, C2HMessageType type, char *user, size_t length) {
  const char kUserNameStartTag  = '[';
  const char kUserNameEndTag  = ']';
  if (type == kMessageTypeMsg || type == kMessageTypeLog) {
    char *start = strchr(message, kUserNameStartTag); // Normally message[5]
    if (start != NULL) {
      // We have a starting point
      start++; // User name starts after the bracket
      char *end = strchr(start, kUserNameEndTag);
      if (end != NULL) {
        size_t userLength = end - start;
        if (userLength > 0 && userLength <= length) {
          memcpy(user, start, userLength);
          *(user + userLength) = 0; // Enforce a NULL terminator
          return true;
        }
      }
    }
  }
  return false;
}

/**
 * Wraps an outgoing message into the given command type
 * @param[in] type Type of command to generate
 * @param[in] dest Destination char array
 * @param[in] size Maximum size of the message to generate
 * @param[in] format printf-compatible formatted string
 * @param[in] ...
 */
void Message_format(unsigned int type, char *dest, size_t size, const char *format, ...) {
  if (dest == NULL) return;

  const char *commandPrefix = "";
  switch (type) {
    case kMessageTypeNick:
      commandPrefix = kMessageTypePrefixNick;
    break;
    case kMessageTypeMsg:
      commandPrefix = kMessageTypePrefixMsg;
    break;
    case kMessageTypeLog:
      commandPrefix = kMessageTypePrefixLog;
    break;
    case kMessageTypeOk:
      commandPrefix = kMessageTypePrefixOk;
    break;
    case kMessageTypeErr:
      commandPrefix = kMessageTypePrefixErr;
    break;
    default:
      return; // Unknown message type
  }

  va_list args;
  va_start(args, format);

  // Compile the message with the provided args in a temporary buffer
  char buffer[size];
  memset(buffer, 0, size);
  vsnprintf(buffer, size, format, args);

  va_end(args);

  // Compile the final message by adding the prefix
  if (strlen(buffer) > 0)
    snprintf(dest, size, "%s %s", commandPrefix, buffer);
  else
    snprintf(dest, size, "%s", commandPrefix);
}

/**
 * Extracts raw message data from a buffer of bytes received by the server
 * After every read, the buffer is updated to point to the next message
 * The returned string needs to be freed with Message_free()
 * @param[in] buffer
 */
char *Message_get(MessageBuffer *buffer) {
  // Start of message
  if (buffer->start == NULL) {
    // Newly created buffer or result of invalid read
    buffer->start = buffer->data;
  }
  // How much we can read from start till the end of the buffer
  size_t size = sizeof(buffer->data) - (buffer->start - buffer->data);
  // Identify the start of a valid message
  buffer->start = memchr(buffer->start, '/', size);
  if (buffer->start == NULL) {
    // No valid data available ahead, already read data behind,
    // a good occasion to cleanup
    memset(buffer->data, 0, sizeof(buffer->data));
    return NULL;
  }

  // Pointer to end of buffer
  char *end = buffer->start + size -1;

  for (char *cursor = buffer->start;; cursor++) {
    if (cursor == end && *cursor != 0 ) {
      return NULL; // No null terminator, partial message
    }
    if (*cursor == 0) {
      end = cursor; // Now points to end of message
      break;
    }
  }
  // Length of message data to process, including the NULL terminator
  size_t length = end - buffer->start + 1;
  if (length <= 1) return NULL; // no more data

  // Copy the message data
  char *message = calloc(1, length);
  if (!message) return NULL;
  memcpy(message, buffer->start, length);

  // Update cursor for next reading
  if (length < size) {
    buffer->start += length;
  }
  if (length == size) {
    // No more to parse, reset
    // Note: this may never happen, unless the data fits perfectly
    // in our buffer, which is very rare
    buffer->start = NULL;
    memset(buffer->data, 0, sizeof(buffer->data));
  }
  return message;
}

// Temporary, since getType is used by the app.c
C2HMessageType Message_getRawType(char **message) {
  // sizeof may be faster than strlen because it is known at compile time
  // but it includes the NULL terminator, so we need -1
  #define RMATCH(prefix) (strncmp(*message, prefix, sizeof(prefix) - 1) == 0)
  if (*message != NULL) {
    if (RMATCH(kMessageTypePrefixNick)) {
      *message += (sizeof(kMessageTypePrefixNick) - 1);
      return kMessageTypeNick;
    }
    if (RMATCH(kMessageTypePrefixMsg)) {
      *message += (sizeof(kMessageTypePrefixMsg) - 1);
      return kMessageTypeMsg;
    }
    if (RMATCH(kMessageTypePrefixQuit)) {
      *message += (sizeof(kMessageTypePrefixQuit) - 1);
      return kMessageTypeQuit;
    }
    if (RMATCH(kMessageTypePrefixLog)) {
      *message += (sizeof(kMessageTypePrefixLog) - 1);
      return kMessageTypeLog;
    }
    if (RMATCH(kMessageTypePrefixErr)) {
      *message += (sizeof(kMessageTypePrefixErr) - 1);
      return kMessageTypeErr;
    }
    if (RMATCH(kMessageTypePrefixOk)) {
      *message += (sizeof(kMessageTypePrefixOk) - 1);
      return kMessageTypeOk;
    }
  }
  return kMessageTypeNull;
}

/**
 * Extracts a message's content into a C2HMessage structure
 * The returned structure needs to be freed with C2HMessage_free()
 * @param[in] buffer
 */
C2HMessage *C2HMessage_get(MessageBuffer *buffer) {
  char *messageData = Message_get(buffer);
  if (messageData == NULL) return NULL;

  C2HMessage *message = calloc(1, sizeof(C2HMessage));
  char *cursor = messageData;
  char *end = messageData + strlen(messageData);

  message->type = Message_getRawType(&cursor);
  // cursor has now been advanced by the length of the type prefix
  if (message->type == kMessageTypeNull) {
    C2HMessage_free(&message);
    Message_free(&messageData);
    return NULL;
  }
  cursor = trim(cursor, NULL);

  // Dealing with optional content
  if (message->type == kMessageTypeOk || message->type == kMessageTypeQuit) {
    if (strlen(cursor) == 0) {
      // No content
      Message_free(&messageData);
      return message;
    }
  }

  if (message->type == kMessageTypeMsg || message->type == kMessageTypeLog) {
    Message_getUser(messageData, message->type, message->user, kMaxNicknameSize);
  }
  int userLength = strlen(message->user);
  if (userLength > 0) cursor += userLength + 3; // Add [] and trailing space
  memcpy(message->content, cursor, end - cursor);
  Message_free(&messageData);
  return message;
}

/**
 * Frees memory space for a message allocated by Message_getContent()
 * @param[in] message
 */
void Message_free(char **message) {
  if (message != NULL && *message != NULL) {
    memset(*message, 0, strlen(*message));
    free(*message);
    *message = NULL;
  }
}

/**
 * Frees memory space for a message allocated by C2HMessage_get()
 * @param[in] message
 */
void C2HMessage_free(C2HMessage **message) {
  if (message != NULL && *message != NULL) {
    memset(*message, 0, sizeof(C2HMessage));
    free(*message);
    *message = NULL;
  }
}
