/*
 * Copyright (C) 2021 Vito Tardia
 */

#ifndef UI_H
#define UI_H

  #include <ncurses.h>

  enum UIInputLoopStatus {
    kUITerminate = 0,
    kUIUpdate = -1,
    kUIResize = -2
  };

  // Initialises the UI engine
  void UIInit();

  // Destroys the UI engine
  void UIClean();

  /**
   * Runs the UI input loop
   *
   * NOTE: the real size of buffer is length * sizeof(wchar_t)
   *
   * param[in] buffer Array of Unicode characters
   * param[in] length Max characters to be stored into the buffer
   */
  int UIInputLoop(wchar_t *buffer, size_t length);

  // Signals the UI to terminate
  // Required to interrupt the input loop
  void UITerminate();

  // Signals the UI to update the chat log
  // Required to interrupt the input loop
  void UIUpdateChatLog();

  // Adds a message to the chat log display buffer
  void UILogMessage(char *buffer, size_t length);

  /**
   * Updates the status bar message
   *
   * The final message format is '[S] [Y,X] Message', where
   *   [S]   => chat window status (live | browse)
   *   [Y,X] => terminal size (lines, columns)
   */
  bool UISetStatus(char *format, ...);
#endif
