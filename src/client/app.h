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

#ifndef APP_H
#define APP_H
  #include "../c2hat.h"
  #include "client.h"
  #include <stddef.h>
  #include <locale.h>

  enum {
    /// Max input buffer used to ask for the user nickname
    kMaxNicknameInputBuffer = 64 * sizeof(wchar_t),
    /// Max message length, in characters, including the NULL terminator
    kMaxMessageLength = 281
  };

  /**
   * Ensures the app is running within an interactive terminal
   */
  #define App_checkTTY() {                                                              \
    if (!isatty(fileno(stdout))) {                                                      \
      fprintf(stderr, "❌ Error: ENOTTY - Invalid terminal\n");                         \
      fprintf(stderr, "Cannot start the C2Hat client in a non-interactive terminal\n"); \
      exit(EXIT_FAILURE);                                                               \
    }                                                                                   \
  }

  /**
   * Checks that the current locale supports UTF-8
   *
   * 1) Calling setlocale() with an empty string loads the LANG env var
   * 2) Calling setlocale() with a NULL argument reads the corresponding LC_ var
   *    If the system does not support the locale it will return "C"
   *    otherwise the full locale string (e.g. en_US.UTF-8)
   * 3) If the locale string does not contain UTF-8 it's a no-go
   */
  #define App_initLocale() {                                                     \
    if (!setlocale(LC_ALL, "")) {                                                \
      fprintf(stderr, "Unable to read locale");                                  \
      exit (EXIT_FAILURE);                                                       \
    }                                                                            \
    char *locale = setlocale(LC_ALL, NULL);                                      \
    if (strstr(locale, "UTF-8") == NULL) {                                       \
      fprintf(stderr, "The given locale (%s) does not support UTF-8\n", locale); \
      exit(EXIT_FAILURE);                                                        \
    }                                                                            \
  }

  /**
   * Initialise global application resources
   */
  void App_init(ClientOptions *options);

  /**
   * Authenticate with the server
   */
  void App_authenticate();

  /**
   * Start the user interface
   */
  int App_start();
#endif
