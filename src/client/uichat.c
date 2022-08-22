#include "uichat.h"
#include <string.h>

/// The external fixed size chat window box
static UIChatWinBox chatWinBox = {};

/// The internal scrollable chat log window
static UIChatWin chatWin = { .status = kChatWinStatusLive };

void UIChatWin_render(const UIScreen *screen, size_t height, const char *title) {
  if (chatWinBox.handle == NULL) {
    // Create the chat window container as a sub window of the main screen:
    // ~80% tall, 100% wide, starts at top left
    chatWinBox.handle = derwin(screen->handle, height, screen->cols, 0, 0);
  }
  if (chatWin.handle == NULL) {
    // Draw the scrollable chat log box, within the chat window
    chatWin.handle = subwin(chatWinBox.handle, (height - 1), (screen->cols - 2), 1, 1);
  }

  // Add border, just top and bottom to avoid breaking the layout
  // when the user inserts emojis
  // win, left side, right side, top side, bottom side,
  // corners: top left, top right, bottom left, bottom right
  wborder(chatWinBox.handle, ' ', ' ', 0, ' ', ' ', ' ', ' ', ' ');
  chatWinBox.height = height;

  // Draw title
  size_t titleLength = strlen(title);
  mvwaddch(chatWinBox.handle, 0, (screen->cols / 2) - titleLength / 2 - 1 , ACS_RTEE);
  mvwaddstr(chatWinBox.handle, 0, (screen->cols / 2) - titleLength / 2, title);
  mvwaddch(chatWinBox.handle, 0, (screen->cols / 2) + titleLength / 2 + 1, ACS_LTEE);
  wrefresh(chatWinBox.handle);

  // Set the internal window as scrollable
  scrollok(chatWin.handle, TRUE);
  leaveok(chatWin.handle, TRUE);
  wrefresh(chatWin.handle);
  getmaxyx(chatWin.handle, chatWin.lines, chatWin.cols);

  // Available display lines
  // (writing on the last line will make the window scroll)
  chatWin.pageSize = chatWin.lines - 1;

  // Add content rendering here
}

void UIChatWin_logMessage(const char *buffer, size_t length) {
  (void)length;
  // 1 Add the message to the messages list/queue
  // 2 Display the message if the chat window is in 'follow' mode
  wprintw(chatWin.handle, "[%s] %s\n", "TIMESTAMP", buffer);
  wrefresh(chatWin.handle);
}

// void UIChatWinBox_destroy() {
//   UIWindow_destroy(chatWinBox.handle);
//   memset(&chatWinBox, 0, sizeof(UIChatWinBox));
// }

void UIChatWin_destroy() {
  UIWindow_destroy(chatWin.handle);
  memset(&chatWin, 0, sizeof(UIChatWin));
  UIWindow_destroy(chatWinBox.handle);
  memset(&chatWinBox, 0, sizeof(UIChatWinBox));
}

UIChatWinStatus UIChatWin_getStatus() {
  return chatWin.status;
}
