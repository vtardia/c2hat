#include "uiwindow.h"

/**
 * Destroys the given window object
 */
void UIWindow_destroy(WINDOW *win) {
  if (win == NULL) return;
  UIWindow_reset(win);
  wrefresh(win);
  delwin(win);
}
