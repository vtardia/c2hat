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
    kMessageTypeNull = 0,
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

  /// Represents a chat message object
  typedef struct {
    C2HMessageType type;
    char content[kBufferSize];
    char user[kMaxNicknameSize];
  } C2HMessage;

  // Gets the next available message from a message buffer
  C2HMessage *C2HMessage_get(MessageBuffer *buffer);

  // Creates a new message given a formatted string
  // e.g. C2Message *hello = C2HMessage_create(kMessageTypeMsg, "Hello %s!", userName);
  C2HMessage *C2HMessage_create(C2HMessageType type, const char *format, ...);

  // Creates a new message given a raw string (e.g. '/msg Hello World!')
  C2HMessage *C2HMessage_createFromString(char *buffer, size_t size);

  // Converts a C2HMessage into a formatted string
  size_t C2HMessage_format(const C2HMessage *message, char *dest, size_t size);

  // Frees memory space for a parsed message
  void C2HMessage_free(C2HMessage **);

#endif
