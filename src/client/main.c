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

/**
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
