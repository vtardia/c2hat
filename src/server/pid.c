/*
 * Copyright (C) 2020 Vito Tardia
 */

/// @file pid.c
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <libgen.h> // for basename() and dirname()
#include <sys/stat.h>
#include <sys/types.h>

#include "logger/logger.h"

static FILE *pidFile = NULL;

/**
 * Tries to open a PID file, inject the current process PID and returns it
 * In case of error it exits the program
 * @param[in] pidFilePath The full PID file path
 */
pid_t PID_init(const char *pidFilePath) {
  FatalIf(
    (access(pidFilePath, F_OK ) != -1), // PID file already exists
    "A PID file (%s) already exists", pidFilePath
  );

  // Try to create the PID directory if not exists
  struct stat st = {};
  char *path = strdup(pidFilePath);
  char *pidDir = dirname(path);
  if (stat(pidDir, &st) == -1) {
    if (mkdir(pidDir, 0700) < 0) {
      if (path != NULL) free(path);
      Fatal("Unable to create PID directory (%d)", pidDir);
    }
  }
  if (path != NULL) free(path);

  pid_t myPID;
  pidFile = fopen(pidFilePath, "w");
  FatalIf(
    (!pidFile),
    "Unable to open PID file '%s': %s", pidFilePath, strerror(errno)
  );

  myPID = getpid();
  fprintf(pidFile, "%d", myPID);
  fclose(pidFile);
  return myPID;
}

/**
 * Loads a PID from a given PID file and returns it
 * In case of error it exits the program
 * @param[in] pidFilePath The full PID file path to load
 */
pid_t PID_load(const char *pidFilePath) {
  char buffer[100] = {};
  pidFile = fopen(pidFilePath, "r");
  FatalIf(
    (!pidFile),
    "Unable to open PID file: %s", strerror(errno)
  );

  fgets(buffer, sizeof(buffer), pidFile);
  fclose(pidFile);
  return atoi(buffer);
}

/**
 * Checks that a PID file exists and it's readable
 * In case of error it exits the program
 * @param[in] pidFilePath The full PID file path to check
 */
void PID_check(const char *pidFilePath) {
  FatalIf(
    (access(pidFilePath, F_OK ) == -1), // There is no PID file
    "Unable to find PID file (%s): the process may not be running",
    pidFilePath
  );
}

/**
 * Checks that a process with the given PID exists
 * Returns 1 if the PID exists
 * Returns 0 if the PID does not exists
 * Returns -1 on error and sets errno
 * @param[in] pid The PID to check
 */
int PID_exists(pid_t pid) {
  int result = kill(pid, 0);
  // PID exists
  if (result == 0) return 1;

  // PID does not exists
  if (result < 0 && errno == ESRCH) {
    return 0;
  }
  // Other error
  return -1;
}

