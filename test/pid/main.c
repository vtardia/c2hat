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

#include <stdio.h>
#include <string.h>

#include "pid.h"
#include "logger/logger.h"

int main(int argc, char const *argv[]) {
  if (argc < 3) return -1;

  vLogInit(LOG_INFO, "/dev/null");

  if (strcmp(argv[1], "init") == 0) {
    pid_t mypid = PID_init(argv[2]);
    if (mypid) return 0;
  }

  if (strcmp(argv[1], "load") == 0) {
    pid_t mypid = PID_load(argv[2]);
    if (mypid) return 0;
  }

  if (strcmp(argv[1], "check") == 0) {
    PID_check(argv[2]);
    return 0;
  }

  return -1;
}
