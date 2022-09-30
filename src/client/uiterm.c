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
