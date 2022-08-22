#include "uiterm.h"
#include <string.h>

void UIScreen_clear() {
  printf("\e[1;1H\e[2J");
}

/**
 * Get the number of available input lines
 * based on the terminal width
 */
int UIGetInputLines(const UIScreen *screen) {
  return (screen->cols < kWideTerminalCols) ? 4  : 3;
}

/**
 * Checks if we have enough space within the terminal
 * We need at least a 24x80 terminal that can host 280
 * characters in the input window
 */
bool UIScreen_isBigEnough(const UIScreen *screen, size_t maxMessageLength) {
  return (
    screen->handle != NULL
      && screen->lines > kMinTerminalLines
      && screen->cols > kMinTerminalCols
      && ((screen->cols - 1) * UIGetInputLines(screen)) >= (int)maxMessageLength
  );
}

/**
 * Displays an error message when the current terminal
 * is too small to contain the chat
 */
void UIRender_terminalTooSmall(const UIScreen *screen) {
  char *message = "Sorry, your terminal is too small!";
  mvwprintw(
    screen->handle,
    (screen->lines / 2), (screen->cols / 2 - strlen(message) / 2),
    "%s", message
  );
  mvwprintw(
    screen->handle,
    (screen->lines / 2 + 1), (screen->cols / 2 - 3),
    "%dx%d", screen->lines, screen->cols
  );
  curs_set(kCursorStateInvisible);
  wrefresh(screen->handle);
}
