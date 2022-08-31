#ifndef UI_CHAT_H
#define UI_CHAT_H
  #include <ncurses.h>
  #include "uiterm.h"

  typedef enum {
    /// The chat window is currently receiving messages in real time
    kChatWinModeLive = 1,
    /// The user is currently navigating the chat window with page keys
    kChatWinModeBrowse = 2
  } UIChatWinMode;

  /// Renders the chat window and its contents
  void UIChatWin_render(const UIScreen *screen, size_t height, const char *title);

  /**
   * Adds a message to the chat window log
   *
   * The message is appended to a list buffer, which is then rendered
   * depending on the chat window mode
   */
  void UIChatWin_logMessage(const char *buffer, size_t length);

  /// Cleanup resources
  void UIChatWin_destroy();

  /// Returns the current display mode of the chat window: live or browse mode
  UIChatWinMode UIChatWin_getMode();

  /// Modifies the display mode of the chat window
  void UIChatWin_setMode(UIChatWinMode mode);

  /// Initialises the chat window and its data structures
  bool UIChatWin_init();

  /// Navigates to the previous screen if browse mode is set and content is available
  void UIChatWin_previousPage();

  /// Navigates to the next screen if browse mode is set and content is available
  void UIChatWin_nextPage();

#endif
