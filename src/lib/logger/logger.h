/**
 * Copyright (C) 2022 Vito Tardia
 *
 * This file is part of vLogger.
 *
 * vLogger is a simple C logging utility which aims to be
 * fast and safe.
 *
 * vLogger is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifndef _V_LOGGER_H_
#define _V_LOGGER_H_

  #include <stdio.h>
  #include <stdbool.h>
  #include <errno.h>
  #include <string.h>
  #include <stdlib.h>

  #if defined(Log) || defined(LogMessage)
    #error There is another log library!
  #endif

  // Define log levels
  // If we have a log level of DEBUG, we will show only levels >= DEBUG
  #define LOG_FATAL 60
  #define LOG_ERROR 50
  #define LOG_WARN  40
  #define LOG_INFO  30
  #define LOG_DEBUG 20
  #define LOG_TRACE 10
  #define LOG_OFF    0

  // Set default level to INFO
  #ifndef LOG_DEFAULT
    #define LOG_DEFAULT LOG_INFO
  #endif

  #define Log(format, ...) Info(format __VA_OPT__(,) __VA_ARGS__)

  #define Trace(format, ...) {                                \
    if (vLogLevel && vLogLevel <= LOG_TRACE) {                \
      vLogMessage("TRACE", format __VA_OPT__(,) __VA_ARGS__); \
    }                                                         \
  }

  #define Debug(format, ...) {                                \
    if (vLogLevel && vLogLevel <= LOG_DEBUG) {                \
      vLogMessage("DEBUG", format __VA_OPT__(,) __VA_ARGS__); \
    }                                                         \
  }

  #define Info(format, ...) {                                \
    if (vLogLevel && vLogLevel <= LOG_INFO) {                \
      vLogMessage("INFO", format __VA_OPT__(,) __VA_ARGS__); \
    }                                                        \
  }
  #define InfoIf(expr, ...) {if (expr) Info(__VA_ARGS__)}

  #define Warn(format, ...) {                                   \
    if (vLogLevel && vLogLevel <= LOG_WARN) {                   \
      vLogMessage("WARNING", format __VA_OPT__(,) __VA_ARGS__); \
    }                                                           \
  }
  #define WarnIf(expr, ...) {if (expr) Warn(__VA_ARGS__)}

  #define Error(format, ...) {                                \
    if (vLogLevel && vLogLevel <= LOG_ERROR) {                \
      vLogMessage("ERROR", format __VA_OPT__(,) __VA_ARGS__); \
    }                                                         \
  }
  #define ErrorIf(expr, ...) {if (expr) Error(__VA_ARGS__)}

  #define Fatal(format, ...) {                                \
    if (vLogLevel && vLogLevel <= LOG_FATAL) {                \
      vLogMessage("FATAL", format __VA_OPT__(,) __VA_ARGS__); \
      exit((errno != 0) ? errno : EXIT_FAILURE);              \
    }                                                         \
  }
  #define FatalIf(expr, ...) {if (expr) Fatal(__VA_ARGS__)}

  /// Contains the global log level
  extern int vLogLevel;

  /**
   * Allows applications to define their own log level
   * and log file destination at runtime
   * @param[in] level One of the log level constants
   * @param[in] filepath Optional log file path, can be NULL
   */
  bool vLogInit(int level, const char* filepath);

  /**
   * Writes a message to the log stream with the given level label
   *
   * Don't use this function directly, use one of the provided
   * macros like Log, Info, Debug, etc that also check for
   * the appropriate log level configuration
   *
   * @param[in] label Log level string label constant
   * @param[in] format printf-style format string
   * @param[in] args Variadic list of arguments
   */
  void vLogMessage(const char *label, const char *format, ...);
#endif
