/*
 * Copyright (C) 2021 Vito Tardia
 *
 * This file is the main entry point of the C2Hat CLI client
 */

#include "app.h"
#include "settings.h"

int main(int argc, ARGV argv) {
  // First check we are running in a terminal (TTY)
  App_checkTTY();

  // Check for UTF-8 support
  App_initLocale();

  // Check command line options and arguments
  ClientOptions options = { .logLevel = LOG_INFO };
  parseOptions(argc, argv, &options);

  // Initialise the application and connects to the server
  App_init(&options);

  // Authenticates with the server
  App_authenticate();

  // Start the user interface
  return App_start();
}
