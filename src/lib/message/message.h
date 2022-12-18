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

#ifndef MESSAGE_H
#define MESSAGE_H
  #include "../c2hat.h"
  #include <stddef.h>
  #include <stdbool.h>

  enum MessageType {
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

  typedef enum MessageType C2HMessageType;

  /// Holds data read from a client's connection
  typedef struct {
    char data[kMessageBufferSize];
    char *start;
  } MessageBuffer;

  typedef struct {
    C2HMessageType type;
    char content[kBufferSize];
    char user[kMaxNicknameSize];
  } C2HMessage;

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

  // Gets the next available message from a message buffer
  C2HMessage *C2HMessage_get(MessageBuffer *buffer);

  // Frees memory space for a parsed message
  void C2HMessage_free(C2HMessage **);

#endif
