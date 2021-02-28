/*
 * Copyright (C) 2020 Vito Tardia
 */

/// @file pid.c
#include <stdlib.h>
#include <stdio.h>

#include "logger/logger.h"

static FILE *pidFile = NULL;

/**
 * Tries to open a PID file, inject the current process PID and returns it
 * In case of error it exits the program
 * @param[in] pidFilePath The full PID file path
 */
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

/**
 * Loads a PID from a given PID file and returns it
 * In case of error it exits the program
 * @param[in] pidFilePath The full PID file path to load
 */
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

/**
 * Checks that a PID file exists and it's readable
 * In case of error it exits the program
 * @param[in] pidFilePath The full PID file path to check
 */
void PID_check(const char *pidFilePath) {
  if(access(pidFilePath, F_OK ) == -1) {
    // There is no PID file
    printf("Unable to find PID file (%s): the process may not be running\n", pidFilePath);
    exit(EXIT_FAILURE);
  }
}

