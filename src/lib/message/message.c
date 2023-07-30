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
 * Frees memory space for a message allocated by Message_getContent()
 * @param[in] message
 */
static void Message_free(char **message) {
  if (message != NULL && *message != NULL) {
    memset(*message, 0, strlen(*message));
    free(*message);
    *message = NULL;
  }
}

/**
 * Finds the user name of a given message
 * The message type must be /msg or /log
 * @param[in] message The server or client message content
 * @param[in] user    Contains the extracted user name
 * @param[in] length The maximum length of the returned content
 */
static bool Message_getUser(const char *message, C2HMessageType type, char *user, size_t length) {
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
static void Message_format(unsigned int type, char *dest, size_t size, const char *format, ...) {
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
    case kMessageTypeQuit:
      commandPrefix = kMessageTypePrefixQuit;
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
 * Formats a C2Message into a string ready for transmission
 * Returns the number of bytes written (including null-terminator)
 * @param[in] message Source message object
 * @param[in] dest    Destination char array
 * @param[in] size    Maximum size of the message to generate (including null-term)
 */
size_t C2HMessage_format(const C2HMessage *message, char *dest, size_t size) {
  if (strlen(message->user) > 0) {
    Message_format(message->type, dest, size, "[%s] %s", message->user, message->content);
  } else {
    Message_format(message->type, dest, size, "%s", message->content);
  }
  return strlen(dest) + 1;
}

/**
 * Extracts raw message data from a buffer of bytes received by the server
 * After every read, the buffer is updated to point to the next message
 * The returned string needs to be freed with Message_free()
 * @param[in] buffer
 */
static char *Message_get(MessageBuffer *buffer) {
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

/**
 * Creates a C2HMessage structure of a known type from a formatted string
 * The returned structure needs to be freed with C2HMessage_free()
 */
C2HMessage *C2HMessage_create(C2HMessageType type, const char *format, ...) {
  C2HMessage *message = calloc(1, sizeof(C2HMessage));
  if (message == NULL) return NULL;

  message->type = type;

  char buffer[kBufferSize] = {};
  char *start = buffer;
  char *end = buffer + kBufferSize -1;

  va_list args;
  va_start(args, format);
  vsnprintf(buffer, kBufferSize, format, args);
  va_end(args);

  if (message->type == kMessageTypeMsg || message->type == kMessageTypeLog) {
    Message_getUser(buffer, type, message->user, kMaxNicknameSize);
  }
  // Remove user from message body
  int userLength = strlen(message->user);
  if (userLength > 0) start += userLength + 3; // Add [] and trailing space
  memcpy(message->content, start, end - start);

  return message;
}

/**
 * Finds the type of a given message
 * NOTE: the input message will be modified by removing the type prefix
 * @param[in] message The server or client (null-terminated) message content
 * @param[out] The message type code or 0 on failure
 */
static C2HMessageType Message_getType(char **message) {
  if (message == NULL) return kMessageTypeNull;

  size_t length = strlen(*message);
  // sizeof may be faster than strlen because it is known at compile time
  // but it includes the NULL terminator, so we need -1
  #define RMATCH(prefix) (length >= (sizeof(prefix) -1) && strncmp(*message, prefix, sizeof(prefix) - 1) == 0)

  if (*message != NULL) {
    if (RMATCH(kMessageTypePrefixOk)) {
      *message += (sizeof(kMessageTypePrefixOk) - 1);
      return kMessageTypeOk;
    }
    if (RMATCH(kMessageTypePrefixMsg)) {
      *message += (sizeof(kMessageTypePrefixMsg) - 1);
      if (strlen(*message) > 0) return kMessageTypeMsg;
    }
    if (RMATCH(kMessageTypePrefixLog)) {
      *message += (sizeof(kMessageTypePrefixLog) - 1);
      if (strlen(*message) > 0) return kMessageTypeLog;
    }
    if (RMATCH(kMessageTypePrefixErr)) {
      *message += (sizeof(kMessageTypePrefixErr) - 1);
      if (strlen(*message) > 0) return kMessageTypeErr;
    }
    if (RMATCH(kMessageTypePrefixQuit)) {
      *message += (sizeof(kMessageTypePrefixQuit) - 1);
      return kMessageTypeQuit;
    }
    if (RMATCH(kMessageTypePrefixNick)) {
      *message += (sizeof(kMessageTypePrefixNick) - 1);
      if (strlen(*message) > 0) return kMessageTypeNick;
    }
  }
  return kMessageTypeNull;
}

/**
 * Parses an input string into a C2HMessage
 * 
 * If the input string includes a message type prefix, it will be used
 * otherwise a standard /msg will be created.
 *
 * The returned structure needs to be freed with C2HMessage_free()
 * @param[in] buffer  The input string
 * @param[in] size    The string's length not including the null-terminator
 */
C2HMessage *C2HMessage_createFromString(char *buffer, size_t size) {
  if (size == 0) return NULL;

  C2HMessageType type = Message_getType(&buffer);
  if (type == kMessageTypeNull) {
    // Regular string, wrap into a message
    return C2HMessage_create(kMessageTypeMsg, "%s", buffer);
  }

  // Limiting user-created messages to available types
  if (type == kMessageTypeMsg || type == kMessageTypeNick || type == kMessageTypeQuit) {

    // Buffer has now been advanced by the length of the type prefix
    char *copyOfBuffer = strdup(buffer); // Or trim will crash
    copyOfBuffer = trim(copyOfBuffer, NULL);

    // Built a message using the parsed type
    C2HMessage *message = C2HMessage_create(type, "%s", copyOfBuffer);
    free(copyOfBuffer);
    return message;
  }
  return NULL;
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
  size_t messageDataLength = strlen(messageData);
  char *end = messageData + messageDataLength;

  message->type = Message_getType(&cursor);
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

#ifdef Test_operations
  #include <assert.h>

  void TestMessage_getType();
  void TestMessage_getContent();
  void TestMessage_format();
  void TestMessage_getUser();
  void TestMessage_get();
  void TestC2HMessage_get();
  void TestC2HMessage_create();
  void TestC2HMessage_createFromString();

  int main(/*int argc, char const *argv[]*/) {
    printf("\n[C2HMessages] Running tests ");

    TestMessage_getType();
    TestMessage_format();
    TestMessage_getUser();
    TestMessage_get();
    TestC2HMessage_get();
    TestC2HMessage_create();
    TestC2HMessage_createFromString();

    printf("DONE!\n\n");
    return EXIT_SUCCESS;
  }

  void TestMessage_getType() {

    // Testing error conditions
    char *untypedMessage = "Untyped message";
    assert(Message_getType(&untypedMessage) == kMessageTypeNull);
    printf(".");
    char *incompleteMessage = "/o";
    assert(Message_getType(&incompleteMessage) == kMessageTypeNull);
    printf(".");
    char *emptyMessage = "";
    assert(Message_getType(&emptyMessage) == kMessageTypeNull);
    printf(".");
    assert(Message_getType(NULL) == kMessageTypeNull);
    printf(".");

    // Testing OK type messages: may have an optional content
    char *successWithMessage = "/ok Successful";
    assert(Message_getType(&successWithMessage) == kMessageTypeOk);
    printf(".");
    char *successWithNoMessage = "/ok";
    assert(Message_getType(&successWithNoMessage) == kMessageTypeOk);
    printf(".");

    // Testing ERR type messages: must have a content
    char *errorWithMessage = "/err Invalid Something";
    assert(Message_getType(&errorWithMessage) == kMessageTypeErr);
    printf(".");
    char *errorWithNoMessage = "/err";
    assert(Message_getType(&errorWithNoMessage) == kMessageTypeNull);
    printf(".");

    // Testing NICK type messages: must have a content
    char *validNickString = "/nick Jason Foo";
    assert(Message_getType(&validNickString) == kMessageTypeNick);
    printf(".");
    char *invalidNickString = "/nick";
    assert(Message_getType(&invalidNickString) == kMessageTypeNull);
    printf(".");

    // Testing MSG type messages: must have a content
    char *validMessageString = "/msg This is a message";
    assert(Message_getType(&validMessageString) == kMessageTypeMsg);
    printf(".");
    char *invalidMessageString = "/msg";
    assert(Message_getType(&invalidMessageString) == kMessageTypeNull);
    printf(".");

    // Testing QUIT type messages: may have an optional (ignored) content
    char *quitMessageWithContent = "/quit Something happened";
    assert(Message_getType(&quitMessageWithContent) == kMessageTypeQuit);
    printf(".");
    char *quitMessageWithNoContent = "/quit";
    assert(Message_getType(&quitMessageWithNoContent) == kMessageTypeQuit);
    printf(".");

    // Testing LOG type messages: must have a content
    char *logMessageWithContent = "/log Something happened";
    assert(Message_getType(&logMessageWithContent) == kMessageTypeLog);
    printf(".");
    char *logMessageWithNoContent = "/log";
    assert(Message_getType(&logMessageWithNoContent) == kMessageTypeNull);
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
#endif
