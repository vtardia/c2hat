/*
 * Copyright (C) 2021 Vito Tardia
 */

#ifndef APP_H
#define APP_H
  #include "client.h"
  #include <stddef.h>

  enum {
    /// Max size of data that can be sent, including the NULL terminator
    kBufferSize = 1536,
    /// Max username length, in characters, excluding the NULL terminator
    kMaxNicknameLength = 15,
    /// Max username size in bytes, for Unicode characters
    kMaxNicknameSize = kMaxNicknameLength * sizeof(wchar_t),
    /// Max input buffer used to ask for the user nickname
    kMaxNicknameInputBuffer = 64 * sizeof(wchar_t)
  };

  /**
   * Initialise global application resources
   * Currently needed for Windows compatibility
   */
  void App_init();

  /**
   * Cleanup resources and exit
   * Currently needed for Windows compatibility
   */
  int App_cleanup(int result);

  /**
   * Set up a signal handler
   */
  int App_catch(int sig, void (*handler)(int));

  /**
   * Handles the given signal by setting the termination flag
   */
  void App_terminate(int signal);

  /**
   * Listens to input from the server and updates the main chat log
   * It needs to be called after UIInit()
   */
  void *App_listen(void *client);

  /**
   * Start the application infinite loop
   * It needs to be called after UIInit()
   */
  void App_run(C2HatClient *this);
#endif
