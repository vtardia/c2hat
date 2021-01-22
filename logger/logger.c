#include "logger.h"

static int logLevel = LOG_DEFAULT_LEVEL;
static FILE *logStream = NULL;
static const int kDateTimeBufferSize = 100;

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

// Get the local time and convert it to a string
void DateTimeNow(char *time_buffer) {
  time_t now = time(NULL);
  int result = strftime(time_buffer, kDateTimeBufferSize, "%c %z", localtime(&now));
  if (result <= 0) {
    time_buffer = "";
  }
}

// Translate numeric log levels into strings
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

void Fatal(const char *format, ...) {
  va_list args;
  va_start(args, format);
  LogMessage(L_FATAL, format, args);
  va_end(args);
  int exit_code = (errno != 0) ? errno : EXIT_FAILURE;
  exit(exit_code);
}

void Error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  LogMessage(L_ERROR, format, args);
  va_end(args);
}

void Warn(const char *format, ...) {
  va_list args;
  va_start(args, format);
  LogMessage(L_WARN, format, args);
  va_end(args);
}

void Info(const char *format, ...) {
  va_list args;
  va_start(args, format);
  LogMessage(L_INFO, format, args);
  va_end(args);
}

void Debug(const char *format, ...) {
  va_list args;
  va_start(args, format);
  LogMessage(L_DEBUG, format, args);
  va_end(args);
}

void Trace(const char *format, ...) {
  va_list args;
  va_start(args, format);
  LogMessage(L_TRACE, format, args);
  va_end(args);
}
