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

#include "uichat.h"
#include <string.h>

#include "hash/hash.h"
#include "list/list.h"
#include "logger/logger.h"
#include "message/message.h"
#include "uilog.h"
#include "uicolor.h"

typedef struct {
  WINDOW *handle;
  int lines;
  int cols;
} UIChatWinBox;

typedef struct {
  WINDOW *handle;
  int lines;
  int cols;
  size_t pageSize;
  int currentLine;
  UIChatWinMode mode;
} UIChatWin;

enum {
  /// Max cached lines for the chat log window
  kMaxCachedLines = 100
};

/// The external fixed size chat window box
static UIChatWinBox wrapper = {};

/// The internal scrollable chat log window
static UIChatWin this = { .mode = kChatWinModeLive };

/// Dynamic list that caches the chatlog content
static List *chatlog = NULL;

/// Associative array where the keys are the user nicknames
static Hash *users = NULL;

/// Keeps track of how many color pairs are available
static int colors = kColorPairGreenOnDefault + 1;

/// Set to true on init if the terminal supports more colors
static bool extendedColors = false;

void UIChatWin_write(ChatLogEntry *entry, bool refresh);

bool UIChatWin_init() {
  // Initialise Users hash
  if (users == NULL) users = Hash_new();
  if (users == NULL) {
    Error("Unable to initialise the users list\n%s\n", strerror(errno));
    return false;
  }

  // Initialise chat buffer
  if (chatlog == NULL) chatlog = List_new();
  if (chatlog == NULL) {
    Error("Unable to initialise the chat log buffer\n%s\n", strerror(errno));
    return false;
  }

  // Initialise colors
  colors = UIColor_getCount();
  extendedColors = (colors > (kColorPairWhiteOnRed + 1));

  return true;
}

/// Updates the content of the chat window with the content of the data buffer
void UIChatWin_updateContent(bool refresh) {
  if (chatlog == NULL || chatlog->length == 0) return;

  // Writing on the last line will make the window scroll
  int start = 0;
  wclear(this.handle);
  wmove(this.handle, 0, 0);

  if (this.mode == kChatWinModeLive) {
    // Display the content of the last page (tail)
    start = chatlog->length - this.pageSize;
    if (start < 0) start = 0;
    for (int line = start; line < chatlog->length; line++) {
      ChatLogEntry *entry = (ChatLogEntry *) List_item(chatlog, line);
      if (entry != NULL) {
        UIChatWin_write(entry, false);
      }
    }
  } else if (this.mode == kChatWinModeBrowse) {
    // Display the content of the current page, updated with page up/down keys
    start = this.currentLine;
    int end = start + this.pageSize;
    if (end >= chatlog->length) end = chatlog->length;
    int line = start;
    while (line < end) {
      ChatLogEntry *entry = (ChatLogEntry *) List_item(chatlog, line);
      if (entry != NULL) {
        UIChatWin_write(entry, false);
      }
      line++;
    }
  }
  if (refresh) wrefresh(this.handle);
}

/// Creates or resizes the chat window
void UIChatWin_render(const UIScreen *screen, size_t height, const char *title) {
  if (wrapper.handle == NULL) {
    // Create the chat window container as a sub window of the main screen:
    // ~80% tall, 100% wide, starts at top left
    wrapper.handle = derwin(screen->handle, height, screen->cols, 0, 0);
  } else {
    // Resize
    UIWindow_reset(wrapper.handle);
    mvderwin(wrapper.handle, 0, 0);
    wresize(wrapper.handle, height, screen->cols);
    wnoutrefresh(wrapper.handle);
  }
  getmaxyx(wrapper.handle, wrapper.lines, wrapper.cols);

  if (this.handle == NULL) {
    // Draw the scrollable chat log box, within the chat window
    this.handle = derwin(wrapper.handle, (wrapper.lines - 1), (wrapper.cols - 2) , 1, 1);
  } else {
    // Resize
    UIWindow_reset(this.handle);
    mvderwin(this.handle, 1, 1);
    wresize(this.handle, (wrapper.lines - 1), (wrapper.cols - 2));
    wnoutrefresh(this.handle);
  }
  getmaxyx(this.handle, this.lines, this.cols);

  // Add border, just top and bottom to avoid breaking the layout
  // when the user inserts emojis
  // win, left side, right side, top side, bottom side,
  // corners: top left, top right, bottom left, bottom right
  wborder(wrapper.handle, ' ', ' ', 0, ' ', ' ', ' ', ' ', ' ');

  // Draw title
  size_t titleLength = strlen(title);
  mvwaddch(wrapper.handle, 0, (screen->cols / 2) - titleLength / 2 - 1 , ACS_RTEE);
  mvwaddstr(wrapper.handle, 0, (screen->cols / 2) - titleLength / 2, title);
  mvwaddch(wrapper.handle, 0, (screen->cols / 2) + titleLength / 2 + 1, ACS_LTEE);
  wrefresh(wrapper.handle);

  // Set the internal window as scrollable
  scrollok(this.handle, TRUE);
  leaveok(this.handle, TRUE);

  // Available display lines
  // (writing on the last line will make the window scroll)
  this.pageSize = this.lines - 1;

  // Update the content if the buffer has data
  UIChatWin_updateContent(false);

  // Refresh everything
  wnoutrefresh(wrapper.handle);
  wnoutrefresh(this.handle);
}

/**
 * Returns a custom color for a given user name
 */
int GetUserColor(char *userName) {
  static int nextColor = -1;
  if (nextColor < 0) {
    // Initialise a default color using a random number
    // from kColorPairDefault to kColorPairGreenOnDefault
    nextColor = rand() % colors;

    // Use extended colors if available
    if (extendedColors) {
      nextColor += kColorPairWhiteOnRed;
    }
  }

  // Check if a user already has a color
  int *color = (int *)Hash_getValue(users, userName);
  if (color == NULL) {
    // First time we see this user, use the next available color
    int userColor = nextColor;

    // Update the next color
    if (++nextColor > colors) nextColor = -1;

    // Add the user's color to the hash
    if (!Hash_set(users, userName, &userColor, sizeof(int))) {
      userColor = 0; // Just in case, we return the default
    }
    return userColor;
  }
  // Return a copy of the value pointed by color
  return *color;
}

/// Writes a single log entry to the chat window, with different formatting
void UIChatWin_write(ChatLogEntry *entry, bool refresh) {
  if (entry == NULL || entry->length == 0) return;

  #define WRITE(color, format, ...) {                       \
    wattron(this.handle, COLOR_PAIR(color));                \
    wprintw(this.handle, format __VA_OPT__(,) __VA_ARGS__); \
    wattroff(this.handle, COLOR_PAIR(color));               \
  }

  switch (entry->type) {
    case kMessageTypeErr:
      WRITE(
        kColorPairWhiteOnRed,
        "[%s] [ERROR] %s\n", entry->timestamp, entry->content
      );
    break;
    case kMessageTypeOk:
      WRITE(
        kColorPairRedOnDefault,
        "[%s] [SERVER] %s\n", entry->timestamp, entry->content
      );
    break;
    case kMessageTypeLog:
      WRITE(
        kColorPairRedOnDefault,
        "[%s] [SERVER] [%s] %s\n", entry->timestamp, entry->username, entry->content
      );
    break;
    case kMessageTypeMsg:
      {
        int userColor = (strlen(entry->username)) ? GetUserColor(entry->username) : kColorPairDefault;
        WRITE(
          userColor,
          "[%s] [%s] %s\n", entry->timestamp, entry->username, entry->content
        );
      }
    break;
    default:
      // Print up to byte_received from the server
      wprintw(
        this.handle, "[%s] Received (%zu bytes): %.*s\n",
        entry->timestamp, entry->length, (int) entry->length, entry->content
      );
    break;
  }
  if (refresh) wrefresh(this.handle);
}

/// Adds a message/entry in the data/log buffer
void UIChatWin_logMessage(const C2HMessage *buffer) {
  if (buffer->type == kMessageTypeQuit) return;

  // Process the server data into a temporary entry
  ChatLogEntry *entry = ChatLogEntry_create(buffer);
  if (entry == NULL) return;

  // Append the entry to the buffer list...
  List_append(chatlog, entry, sizeof(ChatLogEntry));
  // ...and delete the oldest node if we reach the max allowed buffer
  if (chatlog->length > kMaxCachedLines) {
    List_delete(chatlog, 0);
  }

  // Do some other actions based on entry content...

  // Intercept user disconnection message to get user name from the message
  // and remove it from the users hash
  if (entry->type == kMessageTypeLog && strlen(entry->username)
    && strstr(entry->content, "left the chat") != NULL) {
    if (!Hash_delete(users, entry->username)) {
      Error(
        "Unable to remove user '%s' from internal hash",
        entry->username
      );
    }
  }

  // Display the message on the log window if in 'follow' mode
  if (this.mode == kChatWinModeLive) UIChatWin_write(entry, true);

  // Destroy the temporary entry
  ChatLogEntry_free(&entry);
}

/// Cleanup and free resources
void UIChatWin_destroy() {
  UIWindow_destroy(this.handle);
  memset(&this, 0, sizeof(UIChatWin));

  UIWindow_destroy(wrapper.handle);
  memset(&wrapper, 0, sizeof(UIChatWinBox));

  if (users != NULL) Hash_free(&users);
  if (chatlog != NULL) List_free(&chatlog);
}

/// Returns the current display mode of the chatlog window
UIChatWinMode UIChatWin_getMode() {
  return this.mode;
}

/// Updates the display mode of the chat window and refreshes the content
void UIChatWin_setMode(UIChatWinMode mode) {
  if (this.mode == mode) return;

  this.mode = mode;
  if (this.mode == kChatWinModeBrowse) {
    if (chatlog && chatlog->length > this.lines) {
      // Position the line pointer at the start ot the page
      this.currentLine = chatlog->length - this.lines;
    }
    curs_set(kCursorStateInvisible);
  } else /* default to kChatWinModeLive */ {
    if (chatlog && chatlog->length > 0) {
      this.currentLine = chatlog->length -1;
    }
    curs_set(kCursorStateNormal);
  }
  UIChatWin_updateContent(true);
}

/// Displays the previous page of buffered data
void UIChatWin_previousPage() {
  this.currentLine -= this.pageSize;
  if (this.currentLine < 0) this.currentLine = 0;
  UIChatWin_updateContent(true);
}

/// Displays the next page of buffered data
void UIChatWin_nextPage() {
  if (this.currentLine < (int)(chatlog->length - this.pageSize)) {
    this.currentLine += this.pageSize;
    if (this.currentLine > chatlog->length) this.currentLine -= chatlog->length - 1;
    UIChatWin_updateContent(true);
  }
}
