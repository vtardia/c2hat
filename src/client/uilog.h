/*
 * Copyright (C) 2021 Vito Tardia
 */

#ifndef UILOG_H
#define UILOG_H

  #include "../c2hat.h"

  /// Represents a single message in the chat log
  typedef struct {
    char timestamp[15];
    int type;
    char content[kBroadcastBufferSize];
    size_t length;
    char username[kMaxNicknameSize + 1];
  } ChatLogEntry;

  // Creates a log entry from a raw server message
  ChatLogEntry *ChatLogEntry_create(char *buffer, size_t length);

  // Deallocate the memory for a log entry
  void ChatLogEntry_free(ChatLogEntry **entry);

#endif

