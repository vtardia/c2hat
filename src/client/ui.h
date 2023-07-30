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

#ifndef UI_H
#define UI_H

  #include <ncurses.h>
  #include "message/message.h"

  enum UIInputLoopStatus {
    kUITerminate = 0
  };

  // Initialises the UI engine
  void UIInit();

  // Destroys the UI engine
  void UIClean();

  /**
   * Runs the main UI input loop as a first responder
   *
   * Manages errors, interrupts and special characters
   *
   * Returns values:
   *
   *    0   on F1/terminate
   *   >0   on new input message to send
   *   -1   on other error
   *
   * NOTE: the real size of buffer is length * sizeof(wchar_t)
   *
   * param[in] buffer Array of Unicode characters
   * param[in] length Max characters to be stored into the buffer
   * param[in] updateHandler A function to be called to update the chatlog
   */
  int UIInputLoop(wchar_t *buffer, size_t length, void(*updateHandler)());

  // Signals the UI to terminate
  // Required to interrupt the input loop
  void UITerminate();

  /// Signals the UI to trigger a resize
  void UIResize();

  // Signals the UI to update the chat log
  // Required to interrupt the input loop
  void UIUpdateChatLog();

  // Adds a message to the chat log display buffer
  void UILogMessage(const C2HMessage *buffer);

  /**
   * Updates the status bar message
   *
   * The final message format is '[S] [Y,X] Message', where
   *   [S]   => chat window status (live | browse)
   *   [Y,X] => terminal size (lines, columns)
   */
  bool UISetStatus(char *format, ...);
#endif
