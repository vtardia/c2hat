/*
 * Copyright (C) 2020 Vito Tardia
 */

#include "message.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

/**
 * Finds the type of a given message
 * @param[in] message The server or client message content
 * @param[out] The message type code or 0 on failure
 */
int Message_getType(const char *message) {
  if (message != NULL) {
    if (strncmp(message, "/nick ", 6) == 0) {
      return kMessageTypeNick;
    }
    if (strncmp(message, "/msg ", 5) == 0) {
      return kMessageTypeMsg;
    }
    if (strncmp(message, "/quit", 5) == 0) {
      return kMessageTypeQuit;
    }
    if (strncmp(message, "/log ", 5) == 0) {
      return kMessageTypeLog;
    }
    if (strncmp(message, "/err ", 5) == 0) {
      return kMessageTypeErr;
    }
    if (strncmp(message, "/ok", 3) == 0) {
      // Trailing space and additional content is optional on OK
      return kMessageTypeOk;
    }
  }
  return 0;
}

/**
 * Returns the content part of a given message up to the specified length
 * The return value MUST be freed. The value at [length - 1] is the NULL terminator
 * @param[in] message The message to parse
 * @param[in] type The type of message we are extracting
 * @param[in] length The maximum length of the returned content
 * @param[out] The trimmed content of the string
 */
char *Message_getContent(const char *message, unsigned int type, size_t length) {
  // We are assuming here that message is null terminated,
  // the behaviour is undefined otherwise
  if (length == 0 || message == NULL || strlen(message) == 0) return NULL;

  unsigned int prefixLength = 0;
  size_t maxLength = length;
  char *prefix;
  switch (type) {
    case kMessageTypeMsg:
      prefix = "/msg";
    break;
    case kMessageTypeNick:
      prefix = "/nick";
    break;
    case kMessageTypeQuit:
      prefix = "/quit";
    break;
    case kMessageTypeOk:
      prefix = "/ok";
    break;
    case kMessageTypeErr:
      prefix = "/err";
    break;
    case kMessageTypeLog:
      prefix = "/log";
    break;
    default:
      prefix = "";
      return NULL; // Invalid message type
  }
  prefixLength = strlen(prefix);
  maxLength -= prefixLength;
  if (maxLength <= 0 || prefixLength > strlen(message)) return NULL;

  // The message does not contain the prefix
  if (!strstr(message, prefix)) return NULL;

  // Points to start of the parse
  char *pStart = (char *)message + prefixLength;

  // Trimming spaces at the beginning of the string and adjust max length
  const char *spaces = "\t\n\v\f\r ";
  size_t lTrimSize = strspn(pStart, spaces);
  pStart += lTrimSize;
  maxLength -= lTrimSize;

  if (0 >= (signed long)maxLength) return NULL;

  // Now in order to RTrim we need to clone the source
  // because the source is const. We alloc macLength + 1 for the null terminator
  char *buffer = calloc(1, maxLength + 1);
  if (buffer == NULL) return NULL;
  memcpy(buffer, pStart, maxLength);

  // Start now points to the cloned string
  pStart = buffer;

  // End now points to the end of the string
  char *pEnd = pStart + maxLength - 1;

  // Ensure we have at least a null terminator at the end
  // and trim all trailing spaces
  do {
    *pEnd = 0;
    pEnd--;
  } while (pEnd >= pStart && strchr(spaces, *pEnd) != NULL);

  return pStart;
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

  char *commandPrefix = "";
  switch (type) {
    case kMessageTypeNick:
      commandPrefix = "/nick";
    break;
    case kMessageTypeMsg:
      commandPrefix = "/msg";
    break;
    case kMessageTypeLog:
      commandPrefix = "/log";
    break;
    case kMessageTypeOk:
      commandPrefix = "/ok";
    break;
    case kMessageTypeErr:
      commandPrefix = "/err";
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
 * Frees memory space for a message allocated by Message_getContent()
 * @param[in] message
 */
void Message_free(char **message) {
  if (message != NULL) {
    memset(*message, 0, strlen(*message));
    free(*message);
    *message = NULL;
  }
}
