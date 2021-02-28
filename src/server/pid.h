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

#endif
