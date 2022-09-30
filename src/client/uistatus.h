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
