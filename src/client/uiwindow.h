#ifndef UI_WINDOW_H
#define UI_WINDOW_H
  #include <ncurses.h>
  #include "uicolor.h"

#define UIWindow_reset(win) {                      \
  wborder(win, ' ', ' ', ' ',' ',' ',' ',' ',' '); \
  wbkgd(win, COLOR_PAIR(kColorPairDefault));       \
  wclear(win);                                     \
}

  /// Destroys the given window object
  void UIWindow_destroy(WINDOW *win);
#endif
