#include "uiwindow.h"

#include "uicolor.h"

/**
 * Destroys the given window object
 */
void UIWindow_destroy(WINDOW *win) {
  if (win == NULL) return;
  wborder(win, ' ', ' ', ' ',' ',' ',' ',' ',' ');
  wbkgd(win, COLOR_PAIR(kColorPairDefault));
  wclear(win);
  wrefresh(win);
  delwin(win);
}
