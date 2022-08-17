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

  // Runs the UI input loop
  int UIInputLoop();

  // Signals the UI to terminate
  // Required to interrupt the input loop
  void UITerminate();

  // Signals the UI to update the chat log
  // Required to interrupt the input loop
  void UIUpdateChatLog();

  // Adds a message to the chat log display buffer
  void UILogMessage(char *buffer, size_t length);
#endif
