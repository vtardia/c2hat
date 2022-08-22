#ifndef UI_STATUS_H
#define UI_STATUS_H
  #include <ncurses.h>
  #include "uiterm.h"

  enum {
    kMaxStatusMessageSize = 512
  };

  typedef enum {
    kUIStatusPositionLeft = 0,
    kUIStatusPositionRight = 1
  } UIStatusPosition;

  void UIStatusBar_render(const UIScreen *screen);

  void UIStatusBar_destroy();

  /// Updates the status bar text at the given position
  bool UIStatus_set(UIStatusPosition position, char *format, ...);
#endif
