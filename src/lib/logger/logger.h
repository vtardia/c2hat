#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Define log levels
// If we have a log level of DEBUG, we will show only levels >= DEBUG
#define L_FATAL 60
#define L_ERROR 50
#define L_WARN 40
#define L_INFO 30
#define L_DEBUG 20
#define L_TRACE 10
#define L_OFF 0

// Set default level to INFO
#ifndef LOG_DEFAULT_LEVEL
#define LOG_DEFAULT_LEVEL 30
#endif

// Initialise the log engine, returns -1 on failure
// filepath can be NULL, if not null stream will be redirected to that path
int LogInit(int level, FILE *restrict stream, const char* filepath);

// Writes a message to the log stream with the given level
void LogMessage(int level, const char *format, va_list args);

// Writes a Fatal error message to the log stream and terminates the program
void Fatal(const char *format, ...);

// Writes an Error message to the log stream
void Error(const char *format, ...);

// Writes a Warning message to the log stream
void Warn(const char *format, ...);

// Writes an Info message to the log stream
void Info(const char *format, ...);

// Writes a Debug message to the log stream
void Debug(const char *format, ...);

// Writes a Trace message to the log stream
void Trace(const char *format, ...);

#endif
