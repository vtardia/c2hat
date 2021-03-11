/*
 * Copyright (C) 2020 Vito Tardia
 */

#ifndef PID_H
#define PID_H

  #include <sys/types.h>

  // Initialise a PID file with the PID from the current process
  pid_t PID_init(const char *pidFilePath);

  // Load a PID from a PID file
  pid_t PID_load(const char *pidFilePath);

  // Check that a PID file exists and is readable
  void PID_check(const char *pidFilePath);

  // Check that the given PID is used by an existing process
  int PID_exists(pid_t pid);

#endif
