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
