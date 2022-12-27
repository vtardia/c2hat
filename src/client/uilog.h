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

#ifndef UILOG_H
#define UILOG_H

  #include "../c2hat.h"
  #include "message/message.h"

  /// Represents a single message in the chat log
  typedef struct {
    char timestamp[15];
    int type;
    char content[kBroadcastBufferSize];
    size_t length;
    char username[kMaxNicknameSize + 1];
  } ChatLogEntry;

  // Creates a log entry from a raw server message
  ChatLogEntry *ChatLogEntry_create(const C2HMessage *buffer);

  // Deallocate the memory for a log entry
  void ChatLogEntry_free(ChatLogEntry **entry);

#endif

