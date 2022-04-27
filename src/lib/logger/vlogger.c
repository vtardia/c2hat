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

#include "vlogger.h"

#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

enum {
  kDateTimeBufferSize = 100,
  kOutputBufferSize = 1024
};

int vLogLevel = LOG_DEFAULT;

bool vLogInit(int level, const char* filepath) {
  if (level >= LOG_OFF && level <= LOG_FATAL) {
    vLogLevel = level;
  }
  if (filepath != NULL) {
    // Ensure we can write on the destination file, if exists
    if ((access(filepath, F_OK) == 0) && (access(filepath, W_OK) != 0)) {
      return false;
    }
    if (freopen(filepath, "a", stderr) == NULL) {
      // The STDERR is now broken, but errno contains the error code
      return false;
    }
  }
  return true;
}

void vLogMessage(const char *label, const char *format, ...) {
  va_list args;
  va_start(args, format);

  // Generate an ISO 8601 date/time string
  // (e.g. 2022-04-07T16:09:33+0100)
  char timestamp[kDateTimeBufferSize] = {};
  time_t now = time(NULL);
  struct tm lt = {};
  int res = strftime(
    timestamp,
    sizeof(timestamp),
    "%FT%T%z",
    localtime_r(&now, &lt)
  );

  // Generate the first part of the message, by parsing
  // timestamp, pid, thread, label and appending the
  // raw format argument
  char message[kOutputBufferSize] = {};
  snprintf(
    message,
    sizeof(message),
    "%s | %6d | %ld | %-7s | %s\n",
    (res > 0 ? timestamp : ""),
    getpid(),
    (unsigned long) pthread_self(),
    label,
    format
  );

  // Generate the whole message by parsing the remaining
  // part of the format string
  char output[kOutputBufferSize] = {};
  vsnprintf(
    output,
    sizeof(output),
    message,
    args
  );

  va_end(args);

  // Safely write to stream
  write(STDERR_FILENO, output, strlen(output));
}

#ifdef Test_operations
  #include <stdlib.h>
  #include <assert.h>

  int main(/*int argc, char const *argv[]*/) {
    // Used to verify that the PID is written into the log
    pid_t mypid = getpid();
    unsigned long mytid = (unsigned long) pthread_self();

    // Accessible log file to write to
    char *logFilePath = "/tmp/liblogger.log";

    // Read file pointer
    FILE *logReader;

    // Used to read lines from the log file to test
    char line[kOutputBufferSize] = {};

    // Contains an expected string to check for
    char expected[kDateTimeBufferSize] = {};

    printf("\nRunning tests");

    // Fails to initialise the log with a file with no access
    assert(!vLogInit(LOG_INFO, "/var/log/test.log"));
    printf(".");

    // SETUP ensure the log file does not exist at the start of the tests
    assert(access(logFilePath, F_OK) < 0);
    printf(".");

    // Initialises the log to stderr
    assert(vLogInit(LOG_INFO, NULL));
    printf(".");

    // Initialises the log with an accessible file
    assert(vLogInit(LOG_INFO, logFilePath));
    printf(".");

    // Ensure that the file has been created
    assert(access(logFilePath, F_OK) == 0);
    printf(".");

    srand(time(NULL));

    // Write some logs using different facilities
    Info("A simple info message with param: %d", rand());
    Warn("A simple warning message with param: %d", rand());
    Error("A simple error message with param: %d", rand());

    // These should not be logged with the default level
    Debug("A simple debug message with param: %d", rand());
    Trace("A simple trace message with param: %d", rand());

    // Read the log file and start asserting
    logReader = fopen(logFilePath, "r");
    assert(logReader != NULL);
    printf(".");

    fgets(line, kOutputBufferSize, logReader);
    sprintf(expected, " %d | %lu | INFO", mypid, mytid);
    assert(strstr(line, expected) != NULL);
    printf(".");

    fgets(line, kOutputBufferSize, logReader);
    sprintf(expected, " %d | %lu | WARNING", mypid, mytid);
    assert(strstr(line, expected) != NULL);
    printf(".");

    fgets(line, kOutputBufferSize, logReader);
    sprintf(expected, " %d | %lu | ERROR", mypid, mytid);
    assert(strstr(line, expected) != NULL);
    printf(".");

    // There should be no more lines
    assert(fgets(line, kOutputBufferSize, logReader) == NULL);
    printf(".");

    // TEARDOWN(1): remove leftover log file
    fclose(logReader);
    assert(remove(logFilePath) == 0);
    printf(".");

    // Reinit with the same file and a different log level
    assert(vLogInit(LOG_DEBUG, logFilePath));
    printf(".");

    // Ensure that the file has been created
    assert(access(logFilePath, F_OK) == 0);
    printf(".");

    // Write some more lines
    Info("A simple info message with param: %d", rand());
    Warn("A simple warning message with param: %d", rand());
    Error("A simple error message with param: %d", rand());

    // We should see this now
    Debug("A simple debug message with param: %d", rand());

    // This should not be logged
    Trace("A simple trace message with param: %d", rand());

    // Read the file and start asserting
    logReader = fopen(logFilePath, "r");
    assert(logReader != NULL);
    printf(".");

    fgets(line, kOutputBufferSize, logReader);
    sprintf(expected, " %d | %lu | INFO", mypid, mytid);
    assert(strstr(line, expected) != NULL);
    printf(".");

    fgets(line, kOutputBufferSize, logReader);
    sprintf(expected, " %d | %lu | WARNING", mypid, mytid);
    assert(strstr(line, expected) != NULL);
    printf(".");

    fgets(line, kOutputBufferSize, logReader);
    sprintf(expected, " %d | %lu | ERROR", mypid, mytid);
    assert(strstr(line, expected) != NULL);
    printf(".");

    // We should see the debug line now
    fgets(line, kOutputBufferSize, logReader);
    sprintf(expected, " %d | %lu | DEBUG", mypid, mytid);
    assert(strstr(line, expected) != NULL);
    printf(".");

    // There should be no more lines
    assert(fgets(line, kOutputBufferSize, logReader) == NULL);
    printf(".");

    // TEARDOWN(2): remove leftover log file
    fclose(logReader);
    assert(remove(logFilePath) == 0);
    printf(".");

    printf("DONE!\n\n");
    return EXIT_SUCCESS;
  }
#endif
