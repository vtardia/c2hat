/*
 * Copyright (C) 2021 Vito Tardia
 */

#ifndef UI_H
#define UI_H

  #include <ncurses.h>

  // Initialises the UI engine
  void UIInit();

  // Destroys the UI engine
  void UIClean();

  // Intercepts terminal resize events
  void UIResizeHandler(int signal);

  // Intercepts UI close events to unlock the input loop
  void UITerminate(int signal);

  // Initialises the infinite loop
  void UILoopInit();

  // Reads user input
  size_t UIGetUserInput(wchar_t *buffer, size_t length);

  // Displays output from the server in he chat log
  void UILogMessage(char *buffer, size_t length);

  // Displays a message in the status bar
  void UISetStatusMessage(char *buffer, size_t length);
#endif
