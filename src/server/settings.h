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

#ifndef SETTINGS_H
#define SETTINGS_H

  #include <stdlib.h>
  #include "server.h"

  // vLogger conversion utilities
  #define LOG_LEVEL_MATCH(value, level) (strncasecmp(value, level, 5) == 0)

  #define LOG_LEVEL(level, value) { \
    if (LOG_LEVEL_MATCH(value, "trace")) { \
      level = LOG_TRACE; \
    } else if (LOG_LEVEL_MATCH(value, "debug")) { \
      level = LOG_DEBUG; \
    } else if (LOG_LEVEL_MATCH(value, "info")) { \
      level = LOG_INFO; \
    } else if (LOG_LEVEL_MATCH(value, "warn")) { \
      level = LOG_WARN; \
    } else if (LOG_LEVEL_MATCH(value, "error")) { \
      level = LOG_ERROR; \
    } else if (LOG_LEVEL_MATCH(value, "fatal")) { \
      level = LOG_FATAL; \
    } else { \
      level = LOG_DEFAULT; \
    } \
  }

  char *GetConfigFilePath(char *filePath, size_t length);

  char *GetDefaultPidFilePath(char *filePath, size_t length);

  char *GetDefaultLogFilePath(char *filePath, size_t length);

  void GetDefaultTlsFilePaths(
    char *certPath, size_t certLength,
    char *keyPath, size_t keyLength
  );

  char *GetDefaultUsersFilePath(char *filePath, size_t length);

  char *GetWorkingDirectory(char *dirPath, size_t length);

  /// ARGV wrapper for options parsing
  typedef char * const * ARGV;
  int parseOptions(int argc, ARGV argv, ServerConfigInfo *settings);
#endif
