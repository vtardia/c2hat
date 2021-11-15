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
    memset(entry->content, 0, kBroadcastBufferSize);
    strncpy(entry->content, content, kBroadcastBufferSize - 1);
    entry->length = strlen(entry->content);
    Message_free(&content);

    // If the message is empty (e.g /ok with no detail), cleanup and return NULL
    if (entry->length == 0) {
      // Paranoid free, TODO: maybe memset_s?
      memset(entry->content, 0, kBroadcastBufferSize);
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
    memset(*entry, 0, sizeof(ChatLogEntry));
    // Free the pointer to which entry is pointing
    // which is the actual pointer to the object
    free(*entry);
    *entry = NULL;
  }
}


