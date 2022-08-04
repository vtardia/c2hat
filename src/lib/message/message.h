/*
 * Copyright (C) 2020 Vito Tardia
 */

#ifndef MESSAGE_H
#define MESSAGE_H
  #include <stddef.h>
  #include <stdbool.h>

  enum {
    kMessageTypeNick = 100,
    kMessageTypeAuth = 110,
    kMessageTypeHelp = 120,
    kMessageTypeMsg = 130,
    kMessageTypeList = 140,
    kMessageTypeQuit = 150,
    kMessageTypeOk = 160,
    kMessageTypeErr = 170,
    kMessageTypeLog = 180,
    kMessageTypeAdmin = 300
  };

  enum {
    kMessageBufferSize = 2048
  };

  /// Holds data read from a client's connection
  typedef struct {
    char data[kMessageBufferSize];
    char *start;
  } MessageBuffer;

  // Returns the type of a given message
  int Message_getType(const char *message);

  // Returns the user part of a given message
  // The length of user MUST be > length
  bool Message_getUser(const char *message, char *user, size_t length);

  // Returns the content part of a message
  char *Message_getContent(const char *message, unsigned int type, size_t length);

  // Wraps an outgoing message into the given command type
  void Message_format(unsigned int type, char *dest, size_t size, const char *format, ...);

  // Frees memory space for a parsed message
  void Message_free(char **);

  // Gets the next available message from a message buffer
  char *Message_get(MessageBuffer *buffer);

#endif
