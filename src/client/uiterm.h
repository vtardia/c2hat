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

#ifndef UI_TERM_H
#define UI_TERM_H

  #include "uiwindow.h"

  enum TermConfig {
    /// Min lines to be available in the terminal (eg. 24x80 term has 22 lines available)
    kMinTerminalLines = 22,
    /// Min columns to be available in the terminal
    kMinTerminalCols = 80,
    /// Min columns to be considered for a wide terminal
    kWideTerminalCols = 94,
  };

  enum CursorState {
    kCursorStateInvisible = 0,
    kCursorStateNormal = 1,
    kCursorStateVeryVisible = 2
  };

  typedef struct {
    WINDOW *handle;
    int lines;
    int cols;
  } UIScreen;

  /// Clears the console
  void UIScreen_clear();

  #define UIScreen_destroy(screen) {      \
    UIWindow_destroy(screen.handle);      \
    memset(&screen, 0, sizeof(UIScreen)); \
  }

  /**
   * Checks if we have enough space within the terminal
   * We need at least a 24x80 terminal that can host 280
   * characters in the input window
   */
  bool UIScreen_isBigEnough(const UIScreen *screen, size_t maxMessageLength);

  /**
   * Displays an error message when the current terminal
   * is too small to contain the chat
   */
  void UIRender_terminalTooSmall(const UIScreen *screen);

  /**
   * Gets the number of available input lines
   * based on the terminal width
   */
  int UIGetInputLines(const UIScreen *screen);

#endif
