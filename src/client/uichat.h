#ifndef UI_CHAT_H
#define UI_CHAT_H
  #include <ncurses.h>
  #include "uiterm.h"

  typedef enum {
    /// The chat window is currently receiving messages in real time
    kChatWinStatusLive = 1,
    /// The user is currently navigating the chat window with page keys
    kChatWinStatusBrowse = 2
  } UIChatWinStatus;

  /// Renders the chat window and its contents
  void UIChatWin_render(const UIScreen *screen, size_t height, const char *title);

  /**
   * Adds a message to the chat window log
   *
   * The message is appended to a list buffer, which is then rendered
   * depending on the chat window mode
   */
  void UIChatWin_logMessage(const char *buffer, size_t length);

  // void UIChatWinBox_destroy();

  void UIChatWin_destroy();

  /// Returns the current status of the chat window: live or browse mode
  UIChatWinStatus UIChatWin_getStatus();

  /// Initialises the chat window and its data structures
  bool UIChatWin_init();
#endif
