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

#ifndef UI_CHAT_H
#define UI_CHAT_H
  #include <ncurses.h>
  #include "uiterm.h"
  #include "message/message.h"

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
  void UIChatWin_logMessage(const C2HMessage *buffer);

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
