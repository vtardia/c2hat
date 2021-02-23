/*
 * Copyright (C) 2020 Vito Tardia
 */

/// @file logger.c
#include "logger.h"

/// Default log level
static int logLevel = LOG_DEFAULT_LEVEL;

/// Pointer to the destination stream
static FILE *logStream = NULL;

/// Default length for date time strings
static const int kDateTimeBufferSize = 100;

/**
 * Initialises the log engine, returns 0 on success or -1 on failure
 * @param[in] level One of the log level constants
 * @param[in] stream Log destination stream, if null stderr will be used
 * @param[in] filepath Optional path to a logfile to redirect the stream, can be NULL
 */
int LogInit(int level, FILE *restrict stream, const char* filepath) {
  logLevel = level;
  logStream = (stream == NULL) ? stderr : stream;
  if (filepath != NULL) {
    if (freopen(filepath, "a", logStream) == NULL) {
      return -1;
    }
  }
  return 0;
}

/**
 * Gets the local system time and converts it to a string
 * @param[out] time_buffer A char array long at least kDateTimeBufferSize
 */
void DateTimeNow(char *time_buffer) {
  time_t now = time(NULL);
  int result = strftime(time_buffer, kDateTimeBufferSize, "%c %z", localtime(&now));
  if (result <= 0) {
    time_buffer = "";
  }
}

/**
 * Translates numeric log levels into strings
 * @param[in] level The error level int constant
 */
char *GetLogLevelName(int level) {
  switch (level) {
    case L_FATAL: return "FATAL";
    case L_ERROR: return "ERROR";
    case L_WARN: return "WARNING";
    case L_INFO: return "INFO";
    case L_DEBUG: return "DEBUG";
    case L_TRACE: return "TRACE";
    default: return "";
  }
}

/**
 * Writes a message to the log stream with the given level label
 * @param[in] level Log level integer constant
 * @param[in] format printf-style format string
 * @param[in] args Variadic list of arguments
 */
void LogMessage(int level, const char *format, va_list args) {
  char *level_name = GetLogLevelName(level);

  if (level < logLevel) return;

  char time_buffer[kDateTimeBufferSize];
  memset(time_buffer, 0, kDateTimeBufferSize);
  DateTimeNow(time_buffer);

  // Create a copy of the argument list,
  // because vsnprintf() will clean the original one
  va_list args2;
  va_copy(args2, args);

  // Create a char array long enough to contain the parsed arguments formatted
  char buffer[1 + vsnprintf(NULL, 0, format, args)];

  // Loads the data from the locations, defined by 'args2' vlist,
  // converts them to character string equivalents using format
  // and writes the results to a buffer string up to a length.
  vsnprintf(buffer, sizeof buffer, format, args2);

  // Cleanup the second argument list
  va_end(args2);

  if (logStream == NULL) {
    logStream = stderr;
  }

  fprintf(logStream, "%s | %6d | %-7s | %s\n", time_buffer, getpid(), level_name, buffer);
  fflush(logStream);
}

/**
 * Writes a FATAL error-type message to the log stream and exits the whole program
 * @param[in] format printf-style format string
 * @param[in] ...
 */
void Fatal(const char *format, ...) {
  va_list args;
  va_start(args, format);
  LogMessage(L_FATAL, format, args);
  va_end(args);
  int exit_code = (errno != 0) ? errno : EXIT_FAILURE;
  exit(exit_code);
}

/**
 * Writes an ERROR-type message to the log stream
 * @param[in] format printf-style format string
 * @param[in] ...
 */
void Error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  LogMessage(L_ERROR, format, args);
  va_end(args);
}

/**
 * Writes an WARNING-type message to the log stream
 * @param[in] format printf-style format string
 * @param[in] ...
 */
void Warn(const char *format, ...) {
  va_list args;
  va_start(args, format);
  LogMessage(L_WARN, format, args);
  va_end(args);
}

/**
 * Writes an INFO-type message to the log stream
 * @param[in] format printf-style format string
 * @param[in] ...
 */
void Info(const char *format, ...) {
  va_list args;
  va_start(args, format);
  LogMessage(L_INFO, format, args);
  va_end(args);
}

/**
 * Writes an DEBUG-type message to the log stream
 * @param[in] format printf-style format string
 * @param[in] ...
 */
void Debug(const char *format, ...) {
  va_list args;
  va_start(args, format);
  LogMessage(L_DEBUG, format, args);
  va_end(args);
}

/**
 * Writes an TRACE-type message to the log stream
 * @param[in] format printf-style format string
 * @param[in] ...
 */
void Trace(const char *format, ...) {
  va_list args;
  va_start(args, format);
  LogMessage(L_TRACE, format, args);
  va_end(args);
}
