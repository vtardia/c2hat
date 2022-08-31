#ifndef UI_STATUS_H
#define UI_STATUS_H
  #include <ncurses.h>
  #include "uiterm.h"

  enum {
    kMaxStatusMessageSize = 512
  };

  typedef enum {
    kUIStatusMode = 0,
    kUIStatusTerminalSize = 1,
    kUIStatusMessage = 2,
    kUIStatusinputCounter = 3
  } UIStatusArea;

  void UIStatusBar_render(const UIScreen *screen);

  void UIStatusBar_destroy();

  /// Updates the status bar text for the area area
  bool UIStatus_set(UIStatusArea area, char *format, ...);

  /// Retrieves the current status message (left side)
  void UIStatus_get(char *buffer, size_t length);
#endif
