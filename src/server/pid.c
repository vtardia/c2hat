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

/// @file pid.c
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <libgen.h> // for basename() and dirname()
#include <sys/stat.h>
#include <sys/types.h>

#include "logger/logger.h"
#include "fsutil/fsutil.h"

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
  char *path = strdup(pidFilePath);
  if (path == NULL) {
    Fatal("Unable to copy the PID file path: %s", strerror(errno));
  }
  char *pidDir = dirname(path);
  bool havePidDir = TouchDir(pidDir, 0700);
  free(path);
  if (!havePidDir) {
    Fatal("Unable to create pid directory (%d)", pidDir);
  }

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

