/*
 * Copyright (C) 2020 Vito Tardia
 */

#include <stdio.h>
#include <assert.h>
#include <time.h>

#include "logger/logger.h"

int main() {
  // Used to verify that the PID is written into the log
  pid_t mypid = getpid();

  // Accessible log file to write to
  char *logFilePath = "/tmp/liblogger.log";

  // Read file pointer
  FILE *logReader;

  // Used to read lines from the log file to test
  char line[1024] = {0};

  // Contains an expected string to check for
  char expected[25] = {0};

  // SETUP ensure the log file does not exist at the start of the tests
  assert(access(logFilePath, F_OK) < 0);
  printf(".");

  // Initialises the log to stderr
  assert(LogInit(L_INFO, NULL, NULL) == 0);
  printf(".");

  // Initialises the log to stdout
  assert(LogInit(L_INFO, stdout, NULL) == 0);
  printf(".");

  // Fails to initialise the log with a file with no access
  assert(LogInit(L_INFO, NULL, "/var/log/test.log") < 0);
  printf(".");

  // Initialises the log with an accessible file
  assert(LogInit(L_INFO, NULL, logFilePath) == 0);
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

  fgets(line, 1024, logReader);
  sprintf(expected, " %d | INFO", mypid);
  assert(strstr(line, expected) != NULL);
  printf(".");

  fgets(line, 1024, logReader);
  sprintf(expected, " %d | WARNING", mypid);
  assert(strstr(line, expected) != NULL);
  printf(".");

  fgets(line, 1024, logReader);
  sprintf(expected, " %d | ERROR", mypid);
  assert(strstr(line, expected) != NULL);
  printf(".");

  // There should be no more lines
  assert(fgets(line, 1024, logReader) == NULL);
  printf(".");

  fclose(logReader);

  // TEARDOWN(1): remove leftover log file
  assert(remove(logFilePath) == 0);
  printf(".");

  // Reinit with the same file and a different log level
  assert(LogInit(L_DEBUG, NULL, logFilePath) == 0);
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

  fgets(line, 1024, logReader);
  sprintf(expected, " %d | INFO", mypid);
  assert(strstr(line, expected) != NULL);
  printf(".");

  fgets(line, 1024, logReader);
  sprintf(expected, " %d | WARNING", mypid);
  assert(strstr(line, expected) != NULL);
  printf(".");

  fgets(line, 1024, logReader);
  sprintf(expected, " %d | ERROR", mypid);
  assert(strstr(line, expected) != NULL);
  printf(".");

  // We should see the debug line now
  fgets(line, 1024, logReader);
  sprintf(expected, " %d | DEBUG", mypid);
  assert(strstr(line, expected) != NULL);
  printf(".");

  // There should be no more lines
  assert(fgets(line, 1024, logReader) == NULL);
  printf(".");

  fclose(logReader);

  // TEARDOWN(2): remove leftover log file
  assert(remove(logFilePath) == 0);
  printf(".");

  printf("\n");
  return 0;
}
