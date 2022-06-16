/*
 * Copyright (C) 2022 Vito Tardia
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

  char *GetWorkingDirectory(char *dirPath, size_t length);

  /// ARGV wrapper for options parsing
  typedef char * const * ARGV;
  int parseOptions(int argc, ARGV argv, ServerConfigInfo *settings);
#endif
