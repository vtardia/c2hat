#include <stdlib.h>
#include <stdio.h>

#include "logger.h"

static FILE *pidFile = NULL;

// Try to open a PID file, inject the current process PID and return it
pid_t PID_init(const char *pidFilePath) {
  if(access(pidFilePath, F_OK ) != -1) {
    // PID file already exists
    Fatal("A PID file (%s) already exists", pidFilePath);
  }
  pid_t my_pid;
  pidFile = fopen(pidFilePath, "w");
  if (!pidFile) {
    Fatal("Unable to open PID file '%s': %s", pidFilePath, strerror(errno));
  }
  my_pid = getpid();
  fprintf(pidFile, "%d", my_pid);
  fclose(pidFile);
  return my_pid;
}

// Loads a PID from a given PID file
pid_t PID_load(const char *pidFilePath) {
  char buffer[100] = {0};
  pidFile = fopen(pidFilePath, "r");
  if (!pidFile) {
    Fatal("Unable to open PID file: %s", strerror(errno));
  }
  fread(buffer, 100, 100, pidFile);
  fclose(pidFile);
  return atoi(buffer);
}

// Check that a PID file exists and it's readable
void PID_check(const char *pidFilePath) {
  if(access(pidFilePath, F_OK ) == -1) {
    // There is no PID file
    printf("Unable to find PID file (%s): the server may not be running\n", pidFilePath);
    exit(EXIT_FAILURE);
  }
}

